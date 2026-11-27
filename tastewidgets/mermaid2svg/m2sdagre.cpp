/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sdagre.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <array>
#include <functional>
#include <set>

namespace tqtw_mermaid2svg {

namespace {

const double INF = std::numeric_limits<double>::infinity();

DagreNode &node(DagreGraph &g, const std::string &id)
{
    return g.nodes.at(id);
}
// ---- rankdir ---------------------------------------------------------------

void dropDanglingEdges(DagreGraph &g)
{
    std::vector<DagreEdge> kept;
    for (const DagreEdge &e : g.edges) {
        if (g.nodes.count(e.v) && g.nodes.count(e.w)) {
            kept.push_back(e);
        }
    }
    g.edges = kept;
}

void swapForRankdir(DagreGraph &g, const std::string &rankdir, bool pre)
{
    if (rankdir != "LR" && rankdir != "RL") {
        return;
    }
    if (pre) {
        for (auto &entry : g.nodes) {
            std::swap(entry.second.width, entry.second.height);
        }
        for (DagreEdge &e : g.edges) {
            std::swap(e.labelWidth, e.labelHeight);
        }
        return;
    }
    for (auto &entry : g.nodes) {
        DagreNode &n = entry.second;
        std::swap(n.width, n.height);
        std::swap(n.x, n.y);
    }
    for (DagreEdge &e : g.edges) {
        std::swap(e.labelWidth, e.labelHeight);
        for (Point &p : e.points) {
            std::swap(p.first, p.second);
        }
        std::swap(e.labelX, e.labelY);
    }
    for (auto &entry : g.clusters) {
        DagreCluster &c = entry.second;
        std::swap(c.width, c.height);
        std::swap(c.x, c.y);
    }
}

void applyAxisFlips(DagreGraph &g, const std::string &rankdir)
{
    if (rankdir != "BT" && rankdir != "RL") {
        return;
    }
    auto flip = [&rankdir](Point p) -> Point {
        return rankdir == "BT" ? Point { p.first, -p.second } : Point { -p.first, p.second };
    };
    for (auto &entry : g.nodes) {
        const Point f = flip({ entry.second.x, entry.second.y });
        entry.second.x = f.first;
        entry.second.y = f.second;
    }
    for (DagreEdge &e : g.edges) {
        for (Point &p : e.points) {
            p = flip(p);
        }
        const Point l = flip({ e.labelX, e.labelY });
        e.labelX = l.first;
        e.labelY = l.second;
    }
    for (auto &entry : g.clusters) {
        const Point f = flip({ entry.second.x, entry.second.y });
        entry.second.x = f.first;
        entry.second.y = f.second;
    }
}

// ---- acyclic ---------------------------------------------------------------

std::vector<DagreEdge> removeSelfEdges(DagreGraph &g)
{
    std::vector<DagreEdge> kept;
    std::vector<DagreEdge> loops;
    for (std::size_t i = 0; i < g.edges.size(); ++i) {
        if (g.edges[i].v == g.edges[i].w) {
            DagreEdge loop = g.edges[i];
            loop.selfLoop = true;
            loops.push_back(loop);
        } else {
            kept.push_back(g.edges[i]);
        }
    }
    g.edges = kept;
    return loops;
}

void restoreSelfEdges(DagreGraph &g, const std::vector<DagreEdge> &loops)
{
    for (const DagreEdge &loop : loops) {
        auto it = g.nodes.find(loop.v);
        if (it == g.nodes.end()) {
            continue;
        }
        DagreNode &n = it->second;
        const double r = std::max(18.0, n.height / 3);
        const double cx = n.x + n.width / 2;
        const double cy = n.y;
        DagreEdge e = loop;
        e.points = { { cx, cy - r * 0.5 }, { cx + r * 1.4, cy - r }, { cx + r * 1.9, cy }, { cx + r * 1.4, cy + r },
            { cx, cy + r * 0.5 } };
        e.labelX = cx + r * 2.1 + e.labelWidth / 2;
        e.labelY = cy;
        g.edges.push_back(e);
    }
}

std::vector<int> makeAcyclic(DagreGraph &g)
{
    std::set<std::string> visited;
    std::set<std::string> onStack;
    std::vector<int> toReverse;
    std::map<std::string, std::vector<int>> outEdges;
    for (std::size_t i = 0; i < g.edges.size(); ++i) {
        outEdges[g.edges[i].v].push_back(static_cast<int>(i));
    }
    auto visit = [&](const std::string &start) {
        std::vector<std::pair<std::string, std::size_t>> stack;
        stack.push_back({ start, 0 });
        onStack.insert(start);
        visited.insert(start);
        while (!stack.empty()) {
            std::string nodeId = stack.back().first;
            std::size_t index = stack.back().second;
            const std::vector<int> &edges = outEdges[nodeId];
            if (index >= edges.size()) {
                stack.pop_back();
                onStack.erase(nodeId);
                continue;
            }
            stack.back().second = index + 1;
            const int edgeIndex = edges[index];
            const std::string w = g.edges[edgeIndex].w;
            if (onStack.count(w)) {
                toReverse.push_back(edgeIndex);
            } else if (!visited.count(w)) {
                visited.insert(w);
                onStack.insert(w);
                stack.push_back({ w, 0 });
            }
        }
    };
    for (const std::string &id : g.nodeOrder) {
        if (!visited.count(id)) {
            visit(id);
        }
    }
    for (int index : toReverse) {
        std::swap(g.edges[index].v, g.edges[index].w);
        g.edges[index].reversed = true;
    }
    return toReverse;
}

void unreverse(DagreEdge &e)
{
    std::swap(e.v, e.w);
    std::reverse(e.points.begin(), e.points.end());
}

// ---- ranking ---------------------------------------------------------------

void rankNodes(DagreGraph &g)
{
    std::map<std::string, std::vector<int>> inEdges, outEdges;
    for (const auto &entry : g.nodes) {
        inEdges[entry.first];
        outEdges[entry.first];
    }
    for (std::size_t i = 0; i < g.edges.size(); ++i) {
        const DagreEdge &e = g.edges[i];
        if (outEdges.count(e.v)) {
            outEdges[e.v].push_back(static_cast<int>(i));
        }
        if (inEdges.count(e.w)) {
            inEdges[e.w].push_back(static_cast<int>(i));
        }
    }
    // topological order (Kahn)
    std::map<std::string, int> indegree;
    for (const auto &entry : g.nodes) {
        indegree[entry.first] = static_cast<int>(inEdges[entry.first].size());
    }
    std::vector<std::string> queue;
    for (const std::string &id : g.nodeOrder) {
        if (indegree[id] == 0) {
            queue.push_back(id);
        }
    }
    std::vector<std::string> order;
    while (!queue.empty()) {
        const std::string id = queue.front();
        queue.erase(queue.begin());
        order.push_back(id);
        for (int ei : outEdges[id]) {
            const std::string w = g.edges[ei].w;
            if (--indegree[w] == 0) {
                queue.push_back(w);
            }
        }
    }
    for (const auto &entry : g.nodes) {
        if (std::find(order.begin(), order.end(), entry.first) == order.end()) {
            order.push_back(entry.first);
        }
    }
    std::map<std::string, int> ranks;
    for (const auto &entry : g.nodes) {
        ranks[entry.first] = 0;
    }
    for (const std::string &id : order) {
        for (int ei : inEdges[id]) {
            ranks[id] = std::max(ranks[id], ranks[g.edges[ei].v] + g.edges[ei].minlen);
        }
    }
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        const std::string &id = *it;
        if (outEdges[id].empty()) {
            continue;
        }
        int latest = std::numeric_limits<int>::max();
        for (int ei : outEdges[id]) {
            latest = std::min(latest, ranks[g.edges[ei].w] - g.edges[ei].minlen);
        }
        int earliest = 0;
        for (int ei : inEdges[id]) {
            earliest = std::max(earliest, ranks[g.edges[ei].v] + g.edges[ei].minlen);
        }
        if (latest > ranks[id] && latest >= earliest) {
            ranks[id] = latest;
        }
    }
    int lo = 0;
    for (const auto &entry : ranks) {
        lo = std::min(lo, entry.second);
    }
    for (auto &entry : g.nodes) {
        entry.second.rank = ranks[entry.first] - lo;
    }
}

// ---- normalisation ---------------------------------------------------------

std::string commonCluster(const DagreGraph &g, const std::string &v, const std::string &w)
{
    const std::vector<std::string> a = g.clusterChain(v);
    const std::vector<std::string> b = g.clusterChain(w);
    std::string common;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        if (a[i] != b[i]) {
            break;
        }
        common = a[i];
    }
    return common;
}

//: chains keyed by the edge index in the *original* edge list.
std::map<std::size_t, std::vector<std::string>> normalize(DagreGraph &g, std::vector<DagreEdge> &realEdges)
{
    std::map<std::size_t, std::vector<std::string>> chains;
    std::vector<DagreEdge> newEdges;
    int counter = 0;
    for (std::size_t ei = 0; ei < g.edges.size(); ++ei) {
        DagreEdge edge = g.edges[ei];
        const DagreNode &vNode = node(g, edge.v);
        const DagreNode &wNode = node(g, edge.w);
        if (wNode.rank - vNode.rank <= 1) {
            newEdges.push_back(edge);
            continue;
        }
        std::vector<std::string> chain;
        std::string prev = edge.v;
        const int labelRank = (vNode.rank + wNode.rank) / 2;
        for (int r = vNode.rank + 1; r < wNode.rank; ++r) {
            ++counter;
            const std::string dummyId = "__d" + std::to_string(counter) + "_" + std::to_string(ei);
            const bool isLabel = r == labelRank && edge.labelWidth > 0;
            DagreNode &dummy = g.addNode(dummyId, isLabel ? edge.labelWidth : 0.0,
                    isLabel ? edge.labelHeight : 0.0);
            dummy.rank = r;
            dummy.dummy = isLabel ? "edge-label" : "edge";
            const std::string parent = commonCluster(g, edge.v, edge.w);
            if (!parent.empty()) {
                dummy.parent = parent;
                dummy.hasParent = true;
            }
            chain.push_back(dummyId);
            DagreEdge seg;
            seg.v = prev;
            seg.w = dummyId;
            seg.weight = edge.weight;
            newEdges.push_back(seg);
            prev = dummyId;
        }
        DagreEdge seg;
        seg.v = prev;
        seg.w = edge.w;
        seg.weight = edge.weight;
        newEdges.push_back(seg);
        chains[ei] = chain;
    }
    realEdges = g.edges;
    g.edges = newEdges;
    return chains;
}

void denormalize(DagreGraph &g, const std::map<std::size_t, std::vector<std::string>> &chains,
        const std::vector<DagreEdge> &realEdges)
{
    // Rebuild the real edges' points from the dummy chains.
    std::vector<DagreEdge> real = realEdges;
    for (const auto &entry : chains) {
        if (entry.first >= real.size()) {
            continue;
        }
        DagreEdge &edge = real[entry.first];
        edge.points.clear();
        for (const std::string &dummyId : entry.second) {
            const DagreNode &dummy = node(g, dummyId);
            edge.points.push_back({ dummy.x, dummy.y });
            if (dummy.dummy == "edge-label") {
                edge.labelX = dummy.x;
                edge.labelY = dummy.y;
            }
        }
    }
    // Drop dummies.
    std::vector<std::string> toRemove;
    for (const auto &entry : g.nodes) {
        if (!entry.second.dummy.empty()) {
            toRemove.push_back(entry.first);
        }
    }
    for (const std::string &id : toRemove) {
        g.nodes.erase(id);
    }
    g.edges = real;
    for (DagreEdge &edge : g.edges) {
        if (edge.selfLoop || !edge.points.empty()) {
            continue;
        }
        auto v = g.nodes.find(edge.v);
        auto w = g.nodes.find(edge.w);
        if (v != g.nodes.end() && w != g.nodes.end()) {
            edge.labelX = (v->second.x + w->second.x) / 2;
            edge.labelY = (v->second.y + w->second.y) / 2;
        }
    }
}


// ---- ordering --------------------------------------------------------------

using Layers = std::vector<std::vector<std::string>>;

double medianValue(std::vector<int> values)
{
    const std::size_t n = values.size();
    if (n == 0) {
        return -1.0;
    }
    if (n == 1) {
        return values[0];
    }
    if (n == 2) {
        return (values[0] + values[1]) / 2.0;
    }
    const std::size_t mid = n / 2;
    if (n % 2 == 1) {
        return values[mid];
    }
    const double left = values[mid - 1] - values[0];
    const double right = values[n - 1] - values[mid];
    if (left + right == 0) {
        return (values[mid - 1] + values[mid]) / 2.0;
    }
    return (values[mid - 1] * right + values[mid] * left) / (left + right);
}

int countInversions(std::vector<int> &seq)
{
    if (seq.size() < 2) {
        return 0;
    }
    const std::size_t mid = seq.size() / 2;
    std::vector<int> left(seq.begin(), seq.begin() + mid);
    std::vector<int> right(seq.begin() + mid, seq.end());
    int count = countInversions(left) + countInversions(right);
    std::size_t i = 0, j = 0;
    std::vector<int> merged;
    while (i < left.size() && j < right.size()) {
        if (left[i] <= right[j]) {
            merged.push_back(left[i++]);
        } else {
            count += static_cast<int>(left.size() - i);
            merged.push_back(right[j++]);
        }
    }
    while (i < left.size()) merged.push_back(left[i++]);
    while (j < right.size()) merged.push_back(right[j++]);
    seq = merged;
    return count;
}

int countCrossings(const Layers &layers, const std::map<std::string, std::vector<std::pair<std::string, double>>> &succs)
{
    int total = 0;
    for (std::size_t i = 0; i + 1 < layers.size(); ++i) {
        std::map<std::string, int> pos;
        for (std::size_t j = 0; j < layers[i + 1].size(); ++j) {
            pos[layers[i + 1][j]] = static_cast<int>(j);
        }
        std::vector<int> pairs;
        for (const std::string &id : layers[i]) {
            std::vector<int> targets;
            auto it = succs.find(id);
            if (it != succs.end()) {
                for (const auto &t : it->second) {
                    auto p = pos.find(t.first);
                    if (p != pos.end()) {
                        targets.push_back(p->second);
                    }
                }
            }
            std::sort(targets.begin(), targets.end());
            pairs.insert(pairs.end(), targets.begin(), targets.end());
        }
        total += countInversions(pairs);
    }
    return total;
}

void wmedian(Layers &layers, const std::map<std::string, std::vector<std::pair<std::string, double>>> &neighbours,
        bool downward)
{
    const int n = static_cast<int>(layers.size());
    std::vector<int> indices;
    if (downward) {
        for (int i = 1; i < n; ++i) indices.push_back(i);
    } else {
        for (int i = n - 2; i >= 0; --i) indices.push_back(i);
    }
    for (int i : indices) {
        const std::vector<std::string> &fixed = downward ? layers[i - 1] : layers[i + 1];
        std::map<std::string, int> pos;
        for (std::size_t j = 0; j < fixed.size(); ++j) {
            pos[fixed[j]] = static_cast<int>(j);
        }
        std::map<std::string, double> medians;
        for (const std::string &id : layers[i]) {
            std::vector<int> values;
            auto it = neighbours.find(id);
            if (it != neighbours.end()) {
                for (const auto &nb : it->second) {
                    auto p = pos.find(nb.first);
                    if (p != pos.end()) {
                        values.push_back(p->second);
                    }
                }
            }
            std::sort(values.begin(), values.end());
            medians[id] = medianValue(values);
        }
        std::map<std::string, int> current;
        for (std::size_t j = 0; j < layers[i].size(); ++j) {
            current[layers[i][j]] = static_cast<int>(j);
        }
        std::stable_sort(layers[i].begin(), layers[i].end(), [&](const std::string &a, const std::string &b) {
            const double ma = medians[a] >= 0 ? medians[a] : current[a];
            const double mb = medians[b] >= 0 ? medians[b] : current[b];
            if (ma != mb) return ma < mb;
            return current[a] < current[b];
        });
    }
}

int crossPair(const Layers &layers, int i, const std::string &left, const std::string &right,
        const std::map<std::string, std::vector<std::pair<std::string, double>>> &preds,
        const std::map<std::string, std::vector<std::pair<std::string, double>>> &succs)
{
    int total = 0;
    for (int direction : { -1, 1 }) {
        const int j = i + direction;
        if (j < 0 || j >= static_cast<int>(layers.size())) {
            continue;
        }
        const auto &neighbours = direction < 0 ? preds : succs;
        std::map<std::string, int> pos;
        for (std::size_t k = 0; k < layers[j].size(); ++k) {
            pos[layers[j][k]] = static_cast<int>(k);
        }
        auto positions = [&](const std::string &id) {
            std::vector<int> out;
            auto it = neighbours.find(id);
            if (it != neighbours.end()) {
                for (const auto &nb : it->second) {
                    auto p = pos.find(nb.first);
                    if (p != pos.end()) out.push_back(p->second);
                }
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        const std::vector<int> lp = positions(left);
        const std::vector<int> rp = positions(right);
        for (int a : lp) {
            for (int b : rp) {
                if (b < a) total += 1;
            }
        }
    }
    return total;
}

void transpose(Layers &layers, const std::map<std::string, std::vector<std::pair<std::string, double>>> &preds,
        const std::map<std::string, std::vector<std::pair<std::string, double>>> &succs)
{
    bool improved = true;
    int guard = 0;
    while (improved && guard < 4) {
        improved = false;
        ++guard;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            auto &layer = layers[i];
            for (std::size_t j = 0; j + 1 < layer.size(); ++j) {
                const std::string v = layer[j], w = layer[j + 1];
                if (crossPair(layers, static_cast<int>(i), v, w, preds, succs)
                        > crossPair(layers, static_cast<int>(i), w, v, preds, succs)) {
                    std::swap(layer[j], layer[j + 1]);
                    improved = true;
                }
            }
        }
    }
}

std::vector<std::string> compactLayer(const DagreGraph &g, const std::vector<std::string> &layer, int depth)
{
    struct Group
    {
        bool hasKey = false;
        std::string key;
        std::vector<std::string> members;
    };
    std::vector<Group> groups;
    std::map<std::string, std::size_t> index;
    std::map<std::string, int> positions;
    for (std::size_t position = 0; position < layer.size(); ++position) {
        positions[layer[position]] = static_cast<int>(position);
        const std::vector<std::string> chain = g.clusterChain(layer[position]);
        if (depth >= static_cast<int>(chain.size())) {
            Group grp;
            grp.members.push_back(layer[position]);
            groups.push_back(grp);
            continue;
        }
        const std::string key = chain[depth];
        auto it = index.find(key);
        if (it == index.end()) {
            index[key] = groups.size();
            Group grp;
            grp.hasKey = true;
            grp.key = key;
            grp.members.push_back(layer[position]);
            groups.push_back(grp);
        } else {
            groups[it->second].members.push_back(layer[position]);
        }
    }
    std::stable_sort(groups.begin(), groups.end(), [&](const Group &a, const Group &b) {
        double sa = 0, sb = 0;
        for (const std::string &m : a.members) sa += positions[m];
        for (const std::string &m : b.members) sb += positions[m];
        return sa / std::max<std::size_t>(1, a.members.size()) < sb / std::max<std::size_t>(1, b.members.size());
    });
    std::vector<std::string> out;
    for (const Group &grp : groups) {
        if (!grp.hasKey || grp.members.size() == 1) {
            out.insert(out.end(), grp.members.begin(), grp.members.end());
        } else {
            const std::vector<std::string> nested = compactLayer(g, grp.members, depth + 1);
            out.insert(out.end(), nested.begin(), nested.end());
        }
    }
    return out;
}

Layers buildInitialOrder(DagreGraph &g)
{
    int maxRank = 0;
    for (const auto &entry : g.nodes) {
        maxRank = std::max(maxRank, entry.second.rank);
    }
    Layers layers(maxRank + 1);
    for (const std::string &id : g.nodeOrder) {
        layers[g.nodes.at(id).rank].push_back(id);
    }
    // DFS seed.
    std::set<std::string> seen;
    std::map<std::string, int> positions;
    int counter = 0;
    std::map<std::string, std::vector<std::string>> succ;
    for (const DagreEdge &e : g.edges) {
        succ[e.v].push_back(e.w);
    }
    std::vector<std::string> roots;
    if (!layers.empty()) {
        roots = layers[0];
    }
    for (const std::string &id : g.nodeOrder) {
        if (std::find(roots.begin(), roots.end(), id) == roots.end()) {
            roots.push_back(id);
        }
    }
    for (const std::string &root : roots) {
        if (seen.count(root)) {
            continue;
        }
        std::vector<std::string> stack { root };
        while (!stack.empty()) {
            const std::string id = stack.back();
            stack.pop_back();
            if (seen.count(id)) {
                continue;
            }
            seen.insert(id);
            positions[id] = counter++;
            std::vector<std::string> children = succ[id];
            std::reverse(children.begin(), children.end());
            for (const std::string &child : children) {
                if (!seen.count(child)) {
                    stack.push_back(child);
                }
            }
        }
    }
    for (auto &layer : layers) {
        std::stable_sort(layer.begin(), layer.end(), [&](const std::string &a, const std::string &b) {
            const int pa = positions.count(a) ? positions[a] : (1 << 30);
            const int pb = positions.count(b) ? positions[b] : (1 << 30);
            return pa < pb;
        });
    }
    return layers;
}

Layers orderNodes(DagreGraph &g)
{
    Layers layers = buildInitialOrder(g);
    std::map<std::string, std::vector<std::pair<std::string, double>>> preds, succs;
    for (const auto &entry : g.nodes) {
        preds[entry.first];
        succs[entry.first];
    }
    for (const DagreEdge &e : g.edges) {
        if (succs.count(e.v) && preds.count(e.w)) {
            succs[e.v].push_back({ e.w, e.weight });
            preds[e.w].push_back({ e.v, e.weight });
        }
    }
    Layers best = layers;
    int bestCrossings = countCrossings(best, succs);
    for (int i = 0; i < 8; ++i) {
        wmedian(layers, i % 2 == 0 ? preds : succs, i % 2 == 0);
        transpose(layers, preds, succs);
        const int crossings = countCrossings(layers, succs);
        if (crossings < bestCrossings) {
            bestCrossings = crossings;
            best = layers;
        }
        if (crossings == 0) {
            break;
        }
    }
    layers = best;
    if (!g.clusters.empty()) {
        for (auto &layer : layers) {
            layer = compactLayer(g, layer, 0);
        }
    }
    for (const auto &layer : layers) {
        for (std::size_t index = 0; index < layer.size(); ++index) {
            g.nodes[layer[index]].order = static_cast<int>(index);
        }
    }
    return layers;
}


// ---- x coordinates (Brandes-Kopf) ------------------------------------------

using NeighbourMap = std::map<std::string, std::vector<std::string>>;

NeighbourMap neighbourMap(const DagreGraph &g, bool upward)
{
    NeighbourMap out;
    for (const auto &entry : g.nodes) {
        out[entry.first];
    }
    for (const DagreEdge &e : g.edges) {
        if (upward) {
            if (out.count(e.w)) out[e.w].push_back(e.v);
        } else {
            if (out.count(e.v)) out[e.v].push_back(e.w);
        }
    }
    return out;
}

std::set<std::pair<std::string, std::string>> type1Conflicts(const DagreGraph &g, const Layers &layers)
{
    std::set<std::pair<std::string, std::string>> conflicts;
    if (layers.size() < 3) {
        return conflicts;
    }
    const NeighbourMap preds = neighbourMap(g, true);
    for (std::size_t i = 1; i + 1 < layers.size(); ++i) {
        const auto &upper = layers[i];
        const auto &lower = layers[i + 1];
        std::map<std::string, int> posUpper;
        for (std::size_t j = 0; j < upper.size(); ++j) posUpper[upper[j]] = static_cast<int>(j);
        int k0 = 0;
        std::size_t scan = 0;
        for (std::size_t l1 = 0; l1 < lower.size(); ++l1) {
            const std::string &v = lower[l1];
            std::string inner;
            const auto nit = preds.find(v);
            if (nit != preds.end() && g.nodes.at(v).dummy != "") {
                for (const std::string &u : nit->second) {
                    if (g.nodes.count(u) && g.nodes.at(u).dummy != "") {
                        inner = u;
                        break;
                    }
                }
            }
            if (l1 == lower.size() - 1 || !inner.empty()) {
                const int k1 = inner.empty() ? static_cast<int>(upper.size()) - 1 : posUpper[inner];
                while (scan <= l1) {
                    const std::string &nodeId = lower[scan];
                    const auto pit = preds.find(nodeId);
                    if (pit != preds.end()) {
                        for (const std::string &u : pit->second) {
                            auto pu = posUpper.find(u);
                            if (pu != posUpper.end() && (pu->second < k0 || pu->second > k1)) {
                                conflicts.insert({ u, nodeId });
                            }
                        }
                    }
                    ++scan;
                }
                k0 = k1;
            }
        }
    }
    return conflicts;
}

struct Alignment
{
    std::map<std::string, std::string> root;
    std::map<std::string, std::string> align;
};

Alignment verticalAlignment(const DagreGraph &g, const Layers &layers,
        const std::set<std::pair<std::string, std::string>> &conflicts, const std::string &horizontal)
{
    Alignment out;
    for (const auto &entry : g.nodes) {
        out.root[entry.first] = entry.first;
        out.align[entry.first] = entry.first;
    }
    const NeighbourMap preds = neighbourMap(g, true);
    const NeighbourMap succs = neighbourMap(g, false);
    for (std::size_t i = 1; i < layers.size(); ++i) {
        const auto &prev = layers[i - 1];
        const auto &current = layers[i];
        std::map<std::string, int> prevPos;
        for (std::size_t j = 0; j < prev.size(); ++j) prevPos[prev[j]] = static_cast<int>(j);
        double last = -1.0;
        for (const std::string &v : current) {
            std::vector<std::string> neighbours;
            if (preds.count(v)) neighbours.insert(neighbours.end(), preds.at(v).begin(), preds.at(v).end());
            if (succs.count(v)) neighbours.insert(neighbours.end(), succs.at(v).begin(), succs.at(v).end());
            std::vector<std::string> filtered;
            for (const std::string &n : neighbours) {
                if (prevPos.count(n)) filtered.push_back(n);
            }
            if (filtered.empty()) continue;
            std::sort(filtered.begin(), filtered.end(), [&](const std::string &a, const std::string &b) {
                return prevPos[a] < prevPos[b];
            });
            filtered.erase(std::unique(filtered.begin(), filtered.end()), filtered.end());
            const double mid = (filtered.size() - 1) / 2.0;
            std::set<int> ms { static_cast<int>(std::floor(mid)), static_cast<int>(std::ceil(mid)) };
            for (int m : ms) {
                if (out.align[v] != v) break;
                const std::string &w = filtered[m];
                const std::pair<std::string, std::string> pair = horizontal == "l" ? std::make_pair(w, v)
                                                                                   : std::make_pair(v, w);
                const bool conflicted = conflicts.count({ w, v }) || conflicts.count({ v, w });
                if (!conflicted && last < prevPos[w]) {
                    out.align[w] = v;
                    out.root[v] = out.root[w];
                    out.align[v] = out.root[v];
                    last = prevPos[w];
                }
            }
        }
    }
    return out;
}

std::map<std::string, double> horizontalCompaction(const DagreGraph &g, const Layers &layers,
        const Alignment &alignment, const LayoutOptions &opts)
{
    std::map<std::string, std::string> sink;
    std::map<std::string, double> shift;
    std::map<std::string, double> xs;
    std::map<std::string, std::pair<int, int>> pos;
    for (const auto &entry : g.nodes) {
        sink[entry.first] = entry.first;
        shift[entry.first] = INF;
    }
    for (std::size_t i = 0; i < layers.size(); ++i) {
        for (std::size_t j = 0; j < layers[i].size(); ++j) {
            pos[layers[i][j]] = { static_cast<int>(i), static_cast<int>(j) };
        }
    }
    auto sep = [&](const std::string &u, const std::string &v) {
        const DagreNode &nu = g.nodes.at(u);
        const DagreNode &nv = g.nodes.at(v);
        double gap = ((nu.dummy != "" ? opts.edgesep : opts.nodesep) + (nv.dummy != "" ? opts.edgesep : opts.nodesep))
                / 2.0;
        const int depth = static_cast<int>(std::max(g.clusterChain(u).size(), g.clusterChain(v).size()));
        if (g.clusterChain(u) != g.clusterChain(v)) {
            gap += depth * (opts.clusterPadding * 2);
        }
        return (nu.width + nv.width) / 2.0 + gap;
    };
    std::function<void(const std::string &)> placeBlock = [&](const std::string &v) {
        if (xs.count(v)) return;
        xs[v] = 0.0;
        std::string w = v;
        while (true) {
            const auto p = pos[w];
            if (p.second > 0) {
                const std::string pred = layers[p.first][p.second - 1];
                const std::string u = alignment.root.at(pred);
                placeBlock(u);
                if (sink[v] == v) sink[v] = sink[u];
                if (sink[v] != sink[u]) {
                    shift[sink[u]] = std::min(shift[sink[u]], xs[v] - xs[u] - sep(pred, w));
                } else {
                    xs[v] = std::max(xs[v], xs[u] + sep(pred, w));
                }
            }
            w = alignment.align.at(w);
            if (w == v) break;
        }
    };
    for (const auto &entry : g.nodes) {
        if (alignment.root.at(entry.first) == entry.first) {
            placeBlock(entry.first);
        }
    }
    std::map<std::string, double> out;
    for (const auto &entry : g.nodes) {
        const std::string r = alignment.root.at(entry.first);
        double value = xs.count(r) ? xs[r] : 0.0;
        const double delta = shift.count(sink.count(r) ? sink[r] : r) ? shift[sink.count(r) ? sink[r] : r] : INF;
        if (delta < INF) value += delta;
        out[entry.first] = value;
    }
    return out;
}

void assignX(DagreGraph &g, const Layers &layers, const LayoutOptions &opts)
{
    if (layers.empty()) return;
    const auto conflicts = type1Conflicts(g, layers);
    std::map<std::string, std::map<std::string, double>> candidates;
    for (const char *verticalC : { "u", "d" }) {
        const std::string vertical = verticalC;
        for (const char *horizontalC : { "l", "r" }) {
            const std::string horizontal = horizontalC;
            Layers adjusted = layers;
            if (horizontal == "r") {
                for (auto &layer : adjusted) std::reverse(layer.begin(), layer.end());
            }
            if (vertical == "u") {
                std::reverse(adjusted.begin(), adjusted.end());
            }
            const Alignment alignment = verticalAlignment(g, adjusted, conflicts, horizontal);
            std::map<std::string, double> xs = horizontalCompaction(g, adjusted, alignment, opts);
            if (horizontal == "r") {
                for (auto &entry : xs) entry.second = -entry.second;
            }
            candidates[vertical + horizontal] = xs;
        }
    }
    // balance
    const std::string alignKey = opts.align.empty() ? std::string() : opts.align;
    if (!alignKey.empty()) {
        std::string key = alignKey;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        if (candidates.count(key)) {
            for (const auto &entry : candidates[key]) {
                g.nodes.at(entry.first).x = entry.second;
            }
            return;
        }
    }
    std::map<std::string, double> widths;
    for (const auto &entry : candidates) {
        double lo = 0, hi = 0;
        bool first = true;
        for (const auto &n : entry.second) {
            const double s = n.second - g.nodes.at(n.first).width / 2;
            const double e = n.second + g.nodes.at(n.first).width / 2;
            lo = first ? s : std::min(lo, s);
            hi = first ? e : std::max(hi, e);
            first = false;
        }
        widths[entry.first] = hi - lo;
    }
    std::string smallest = "ul";
    for (const auto &entry : widths) {
        if (widths[smallest] > entry.second) smallest = entry.first;
    }
    double targetLo = 0, targetHi = 0;
    {
        bool first = true;
        for (const auto &n : candidates[smallest]) {
            const double s = n.second - g.nodes.at(n.first).width / 2;
            const double e = n.second + g.nodes.at(n.first).width / 2;
            targetLo = first ? s : std::min(targetLo, s);
            targetHi = first ? e : std::max(targetHi, e);
            first = false;
        }
    }
    for (auto &entry : candidates) {
        double lo = 0, hi = 0;
        bool first = true;
        for (const auto &n : entry.second) {
            const double s = n.second - g.nodes.at(n.first).width / 2;
            const double e = n.second + g.nodes.at(n.first).width / 2;
            lo = first ? s : std::min(lo, s);
            hi = first ? e : std::max(hi, e);
            first = false;
        }
        const double delta = entry.first[1] == 'l' ? (targetLo - lo) : (targetHi - hi);
        for (auto &n : entry.second) n.second += delta;
    }
    for (auto &entry : g.nodes) {
        std::vector<double> values;
        for (const char *kc : { "ul", "ur", "dl", "dr" }) {
            const std::string key = kc;
            values.push_back(candidates[key][entry.first]);
        }
        std::sort(values.begin(), values.end());
        entry.second.x = (values[1] + values[2]) / 2.0;
    }
}

// ---- y coordinates ---------------------------------------------------------

void assignY(DagreGraph &g, const Layers &layers, double ranksep, const LayoutOptions &opts)
{
    const double pad = opts.clusterPaddingY < 0 ? opts.clusterPadding : opts.clusterPaddingY;
    double y = 0;
    std::set<std::string> previous;
    for (const auto &layer : layers) {
        if (layer.empty()) continue;
        std::set<std::string> enclosing;
        for (const std::string &n : layer) {
            for (const std::string &c : g.clusterChain(n)) enclosing.insert(c);
        }
        if (y != 0) {
            std::set<std::string> symDiff;
            for (const std::string &c : previous) if (!enclosing.count(c)) symDiff.insert(c);
            for (const std::string &c : enclosing) if (!previous.count(c)) symDiff.insert(c);
            y += pad * static_cast<double>(symDiff.size());
        }
        previous = enclosing;
        double maxHeight = 0;
        for (const std::string &n : layer) maxHeight = std::max(maxHeight, g.nodes.at(n).height);
        for (const std::string &n : layer) {
            g.nodes.at(n).y = y + maxHeight / 2.0;
        }
        y += maxHeight + ranksep;
    }
}

// ---- clusters, translate, bounds -------------------------------------------

std::vector<std::string> clusterChainOf(const DagreGraph &g, const std::string &clusterId)
{
    std::vector<std::string> chain;
    auto it = g.clusters.find(clusterId);
    if (it == g.clusters.end() || !it->second.hasParent) return chain;
    std::string current = it->second.parent;
    int guard = 0;
    while (!current.empty() && guard < 64) {
        chain.push_back(current);
        auto cit = g.clusters.find(current);
        if (cit == g.clusters.end() || !cit->second.hasParent) break;
        current = cit->second.parent;
        ++guard;
    }
    return chain;
}

void computeClusters(DagreGraph &g, const LayoutOptions &opts)
{
    if (g.clusters.empty()) return;
    std::map<std::string, std::vector<std::array<double, 4>>> members;
    for (const auto &entry : g.nodes) {
        for (const std::string &cid : g.clusterChain(entry.first)) {
            members[cid].push_back({ entry.second.x - entry.second.width / 2,
                entry.second.y - entry.second.height / 2, entry.second.x + entry.second.width / 2,
                entry.second.y + entry.second.height / 2 });
        }
    }
    std::vector<std::string> ids;
    for (const auto &entry : g.clusters) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end(), [&](const std::string &a, const std::string &b) {
        return clusterChainOf(g, a).size() > clusterChainOf(g, b).size();
    });
    for (const std::string &cid : ids) {
        DagreCluster &cluster = g.clusters[cid];
        std::vector<std::array<double, 4>> boxes = members[cid];
        for (const auto &other : g.clusters) {
            if (other.second.hasParent && other.second.parent == cid && other.second.width) {
                boxes.push_back({ other.second.x - other.second.width / 2, other.second.y - other.second.height / 2,
                    other.second.x + other.second.width / 2, other.second.y + other.second.height / 2 });
            }
        }
        if (boxes.empty()) {
            cluster.width = cluster.height = 0.0;
            continue;
        }
        const double padX = opts.clusterPaddingX < 0 ? opts.clusterPadding : opts.clusterPaddingX;
        const double padY = opts.clusterPaddingY < 0 ? opts.clusterPadding : opts.clusterPaddingY;
        double x0 = boxes[0][0], y0 = boxes[0][1], x1 = boxes[0][2], y1 = boxes[0][3];
        for (const auto &b : boxes) {
            x0 = std::min(x0, b[0]); y0 = std::min(y0, b[1]);
            x1 = std::max(x1, b[2]); y1 = std::max(y1, b[3]);
        }
        x0 -= padX; y0 -= padY + cluster.labelHeight; x1 += padX; y1 += padY;
        if (cluster.minWidth && x1 - x0 < cluster.minWidth) {
            const double grow = (cluster.minWidth - (x1 - x0)) / 2;
            x0 -= grow; x1 += grow;
        }
        cluster.x = (x0 + x1) / 2;
        cluster.y = (y0 + y1) / 2;
        cluster.width = x1 - x0;
        cluster.height = y1 - y0;
    }
}

bool edgeLabelBox(const DagreEdge &edge, std::array<double, 4> *box)
{
    if ((edge.labelWidth == 0 && edge.labelHeight == 0) || (edge.points.empty() && edge.labelX == 0 && edge.labelY == 0)) {
        return false;
    }
    const double hw = edge.labelWidth / 2, hh = edge.labelHeight / 2;
    *box = { edge.labelX - hw, edge.labelY - hh, edge.labelX + hw, edge.labelY + hh };
    return true;
}

void translateToOrigin(DagreGraph &g)
{
    std::vector<double> xs, ys;
    for (const auto &entry : g.nodes) {
        xs.push_back(entry.second.x - entry.second.width / 2);
        xs.push_back(entry.second.x + entry.second.width / 2);
        ys.push_back(entry.second.y - entry.second.height / 2);
        ys.push_back(entry.second.y + entry.second.height / 2);
    }
    for (const auto &entry : g.clusters) {
        if (entry.second.width) {
            xs.push_back(entry.second.x - entry.second.width / 2);
            xs.push_back(entry.second.x + entry.second.width / 2);
            ys.push_back(entry.second.y - entry.second.height / 2);
            ys.push_back(entry.second.y + entry.second.height / 2);
        }
    }
    for (const DagreEdge &e : g.edges) {
        std::array<double, 4> box;
        if (edgeLabelBox(e, &box)) {
            xs.push_back(box[0]); xs.push_back(box[2]);
            ys.push_back(box[1]); ys.push_back(box[3]);
        }
    }
    if (xs.empty()) return;
    const double dx = -*std::min_element(xs.begin(), xs.end());
    const double dy = -*std::min_element(ys.begin(), ys.end());
    for (auto &entry : g.nodes) { entry.second.x += dx; entry.second.y += dy; }
    for (auto &entry : g.clusters) { entry.second.x += dx; entry.second.y += dy; }
    for (DagreEdge &e : g.edges) {
        for (Point &p : e.points) { p.first += dx; p.second += dy; }
        e.labelX += dx; e.labelY += dy;
    }
}

void finalizeBounds(DagreGraph &g, const LayoutOptions &opts)
{
    std::vector<double> xs, ys;
    for (const auto &entry : g.nodes) {
        xs.push_back(entry.second.x - entry.second.width / 2);
        xs.push_back(entry.second.x + entry.second.width / 2);
        ys.push_back(entry.second.y - entry.second.height / 2);
        ys.push_back(entry.second.y + entry.second.height / 2);
    }
    for (const auto &entry : g.clusters) {
        if (entry.second.width) {
            xs.push_back(entry.second.x - entry.second.width / 2);
            xs.push_back(entry.second.x + entry.second.width / 2);
            ys.push_back(entry.second.y - entry.second.height / 2);
            ys.push_back(entry.second.y + entry.second.height / 2);
        }
    }
    for (const DagreEdge &e : g.edges) {
        for (const Point &p : e.points) { xs.push_back(p.first); ys.push_back(p.second); }
        std::array<double, 4> box;
        if (edgeLabelBox(e, &box)) {
            xs.push_back(box[0]); xs.push_back(box[2]);
            ys.push_back(box[1]); ys.push_back(box[3]);
        }
    }
    if (xs.empty()) { g.width = g.height = 0; return; }
    const double minX = *std::min_element(xs.begin(), xs.end());
    const double minY = *std::min_element(ys.begin(), ys.end());
    const double dx = opts.marginX - minX;
    const double dy = opts.marginY - minY;
    for (auto &entry : g.nodes) { entry.second.x += dx; entry.second.y += dy; }
    for (auto &entry : g.clusters) { entry.second.x += dx; entry.second.y += dy; }
    for (DagreEdge &e : g.edges) {
        for (Point &p : e.points) { p.first += dx; p.second += dy; }
        e.labelX += dx; e.labelY += dy;
    }
    g.width = *std::max_element(xs.begin(), xs.end()) + dx + opts.marginX;
    g.height = *std::max_element(ys.begin(), ys.end()) + dy + opts.marginY;
}

} // namespace

// ---- graph construction ----------------------------------------------------

DagreNode &DagreGraph::addNode(const std::string &id, double width, double height)
{
    auto it = nodes.find(id);
    if (it == nodes.end()) {
        DagreNode n;
        n.id = id;
        n.width = width;
        n.height = height;
        nodeOrder.push_back(id);
        return nodes.emplace(id, n).first->second;
    }
    it->second.width = std::max(it->second.width, width);
    it->second.height = std::max(it->second.height, height);
    return it->second;
}

DagreCluster &DagreGraph::addCluster(const std::string &id, const std::string &label)
{
    auto it = clusters.find(id);
    if (it == clusters.end()) {
        DagreCluster c;
        c.id = id;
        c.label = label;
        return clusters.emplace(id, c).first->second;
    }
    return it->second;
}

DagreEdge &DagreGraph::addEdge(const std::string &v, const std::string &w)
{
    DagreEdge e;
    e.v = v;
    e.w = w;
    edges.push_back(e);
    return edges.back();
}

std::vector<std::string> DagreGraph::clusterChain(const std::string &nodeId) const
{
    std::vector<std::string> chain;
    auto it = nodes.find(nodeId);
    if (it == nodes.end() || !it->second.hasParent) {
        return chain;
    }
    std::string current = it->second.parent;
    int guard = 0;
    while (!current.empty() && guard < 64) {
        chain.push_back(current);
        auto cit = clusters.find(current);
        if (cit == clusters.end() || !cit->second.hasParent) {
            break;
        }
        current = cit->second.parent;
        ++guard;
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}


void dagreLayout(DagreGraph &g, LayoutOptions options)
{
    std::string rankdir = options.rankdir.empty() ? "TB" : options.rankdir;
    std::transform(rankdir.begin(), rankdir.end(), rankdir.begin(), [](unsigned char c) { return std::toupper(c); });

    dropDanglingEdges(g);
    swapForRankdir(g, rankdir, true);

    const std::vector<DagreEdge> selfEdges = removeSelfEdges(g);

    const std::vector<int> reversedEdges = makeAcyclic(g);

    const double ranksep = options.ranksep / 2.0;
    for (DagreEdge &e : g.edges) {
        e.minlen *= 2;
    }

    rankNodes(g);
    std::vector<DagreEdge> realEdges;
    const std::map<std::size_t, std::vector<std::string>> chains = normalize(g, realEdges);
    const Layers layers = orderNodes(g);
    assignX(g, layers, options);
    assignY(g, layers, ranksep, options);
    denormalize(g, chains, realEdges);
    restoreSelfEdges(g, selfEdges);
    for (int index : reversedEdges) {
        if (index < static_cast<int>(g.edges.size())) {
            unreverse(g.edges[index]);
        }
    }
    for (DagreEdge &e : g.edges) {
        e.minlen = std::max(1, e.minlen / 2);
    }

    computeClusters(g, options);
    translateToOrigin(g);
    swapForRankdir(g, rankdir, false);
    applyAxisFlips(g, rankdir);
    finalizeBounds(g, options);
}

} // namespace tqtw_mermaid2svg