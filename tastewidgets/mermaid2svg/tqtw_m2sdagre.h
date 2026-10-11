/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// A layered (Sugiyama) graph layout: a C++ port of mermaid2svg's
// layout/dagre.py. Cycle breaking, longest-path ranking with tightening, long
// edges normalised to dummy chains, weighted-median ordering with transpose,
// Brandes-Kopf x-coordinates, then y stacking.

#include "tqtw_m2scurves.h"

#include <map>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

struct DagreNode
{
    std::string id;
    double width = 0;
    double height = 0;
    double x = 0;
    double y = 0;
    int rank = 0;
    int order = 0;
    std::string dummy; // "" | "edge" | "edge-label" | "border"
    std::string parent;
    bool hasParent = false;
};

struct DagreEdge
{
    std::string v;
    std::string w;
    int minlen = 1;
    double weight = 1.0;
    double labelWidth = 0;
    double labelHeight = 0;
    int data = 0;
    std::vector<Point> points;
    double labelX = 0;
    double labelY = 0;
    bool reversed = false;
    bool selfLoop = false;
};

struct DagreCluster
{
    std::string id;
    std::string label;
    std::string parent;
    bool hasParent = false;
    double padding = 8.0;
    double labelHeight = 0;
    double minWidth = 0;
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

struct LayoutOptions
{
    std::string rankdir = "TB";
    double nodesep = 50.0;
    double edgesep = 10.0;
    double ranksep = 50.0;
    double clusterPadding = 8.0;
    double clusterPaddingX = -1; // <0: use clusterPadding
    double clusterPaddingY = -1;
    double marginX = 0;
    double marginY = 0;
    std::string align; // "" | "UL" | "UR" | "DL" | "DR"
};

//: A compound directed graph.
class DagreGraph
{
public:
    //: Keyed by id; the map keeps node pointers stable for the layout.
    std::map<std::string, DagreNode> nodes;
    //: Node ids in insertion order. The layout's DFS and queue passes must
    //: iterate in the order the caller added nodes to match the reference,
    //: which uses an insertion-ordered dict.
    std::vector<std::string> nodeOrder;
    std::vector<DagreEdge> edges;
    std::map<std::string, DagreCluster> clusters;
    double width = 0;
    double height = 0;

    DagreNode &addNode(const std::string &id, double width = 0, double height = 0);
    DagreCluster &addCluster(const std::string &id, const std::string &label = std::string());
    DagreEdge &addEdge(const std::string &v, const std::string &w);
    std::vector<std::string> clusterChain(const std::string &nodeId) const;
};

//: Lay the graph out in place.
void dagreLayout(DagreGraph &graph, LayoutOptions options = LayoutOptions());

} // namespace tqtw_mermaid2svg
