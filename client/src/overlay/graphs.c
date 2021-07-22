/**
 * Looking Glass
 * Copyright (C) 2017-2021 The Looking Glass Authors
 * https://looking-glass.io
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "interface/overlay.h"
#include "cimgui.h"

#include "../main.h"

#include "ll.h"
#include "common/debug.h"

struct GraphState
{
  struct ll * graphs;
};

static struct GraphState gs = {0};

struct OverlayGraph
{
  const char * name;
  RingBuffer   buffer;
  bool         enabled;
};

static bool graphs_init(void ** udata, void * params)
{
  gs.graphs = ll_new();
  return true;
}

static void graphs_free(void * udata)
{
  struct OverlayGraph * graph;
  while(ll_shift(gs.graphs, (void **)&graph))
    free(graph);
  ll_free(gs.graphs);
}

struct BufferMetrics
{
  float min;
  float max;
  float sum;
  float avg;
  float freq;
};

static bool rbCalcMetrics(int index, void * value_, void * udata_)
{
  float * value = value_;
  struct BufferMetrics * udata = udata_;

  if (index == 0)
  {
    udata->min = *value;
    udata->max = *value;
    udata->sum = *value;
    return true;
  }

  if (udata->min > *value)
    udata->min = *value;

  if (udata->max < *value)
    udata->max = *value;

  udata->sum += *value;
  return true;
}

static int graphs_render(void * udata, bool interactive,
    struct Rect * windowRects, int maxRects)
{
  if (!g_state.showTiming)
    return 0;

  ImVec2 pos = {0.0f, 0.0f};
  igSetNextWindowBgAlpha(0.4f);
  igSetNextWindowPos(pos, 0, pos);

  igBegin(
    "Performance Metrics",
    NULL,
    ImGuiWindowFlags_NoDecoration    | ImGuiWindowFlags_AlwaysAutoResize   |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
    ImGuiWindowFlags_NoNav           | ImGuiWindowFlags_NoTitleBar
  );

  GraphHandle graph;
  for (ll_reset(gs.graphs); ll_walk(gs.graphs, (void **)&graph); )
  {
    if (!graph->enabled)
      continue;

    struct BufferMetrics metrics = {};
    ringbuffer_forEach(graph->buffer, rbCalcMetrics, &metrics);

    if (metrics.sum > 0.0f)
    {
      metrics.avg  = metrics.sum / ringbuffer_getCount(graph->buffer);
      metrics.freq = 1000.0f / metrics.avg;
    }

    char  title[64];
    const ImVec2 size = {400.0f, 100.0f};

    snprintf(title, sizeof(title),
        "%s: min:%4.2f max:%4.2f avg:%4.2f/%4.2fHz",
        graph->name, metrics.min, metrics.max, metrics.avg, metrics.freq);

    igPlotLinesFloatPtr(
        "",
        (float *)ringbuffer_getValues(graph->buffer),
        ringbuffer_getLength(graph->buffer),
        ringbuffer_getStart (graph->buffer),
        title,
        0.0f,
        50.0f,
        size,
        sizeof(float));
  };

  if (maxRects == 0)
  {
    igEnd();
    return -1;
  }

  ImVec2 size;
  igGetWindowPos(&pos);
  igGetWindowSize(&size);
  windowRects[0].x = pos.x;
  windowRects[0].y = pos.y;
  windowRects[0].w = size.x;
  windowRects[0].h = size.y;

  igEnd();
  return 1;
}

struct LG_OverlayOps LGOverlayGraphs =
{
  .name           = "Graphs",
  .init           = graphs_init,
  .free           = graphs_free,
  .render         = graphs_render
};

GraphHandle overlayGraph_register(const char * name, RingBuffer buffer)
{
  struct OverlayGraph * graph = malloc(sizeof(struct OverlayGraph));
  graph->name    = name;
  graph->buffer  = buffer;
  graph->enabled = true;
  ll_push(gs.graphs, graph);
  return graph;
}

void overlayGraph_unregister(GraphHandle handle)
{
  handle->enabled = false;
}
