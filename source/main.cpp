#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <commdlg.h>
#include <gdiplus.h>
#include <objidl.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>

#include "scheme_model.h"

namespace
{
using namespace Gdiplus;
constexpr float paletteWidth = 224;
constexpr float paletteTabWidth = 28;
constexpr float headerHeight = 64;
constexpr float footerHeight = 30;
constexpr float blockWidth = 172;
constexpr int commandNew = 101;
constexpr int commandOpen = 102;
constexpr int commandSave = 103;
constexpr int commandSaveAs = 104;
constexpr int commandDelete = 105;
constexpr int commandFit = 106;
constexpr int commandExit = 107;

struct PortHit
{
  int block = -1;
  int port = -1;
  bool output = false;
};

struct Editor
{
  HWND window = nullptr;
  scheme::Graph graph;
  float dpi = 1;
  float width = 1200;
  float height = 760;
  float zoom = 1;
  PointF pan{36, 40};
  PointF mouse{};
  PointF dragOffset{};
  int palette = -1;
  int selected = -1;
  int selectedWire = -1;
  int dragging = -1;
  bool paletteDrag = false;
  bool paletteVisible = true;
  bool panning = false;
  bool dirty = false;
  PortHit pending;
  std::wstring path;
  std::wstring status = L"Ready";
} editor;

std::wstring wide(const std::string& text)
{
  return {text.begin(), text.end()};
}

float canvasLeft()
{
  return editor.paletteVisible ? paletteWidth : paletteTabWidth;
}

RectF paletteTab()
{
  return {canvasLeft() - paletteTabWidth, 82, paletteTabWidth, 28};
}

float blockHeight(const scheme::Block& block)
{
  const auto& type = scheme::blockTypes()[block.type];
  return 48 + 28 * static_cast<float>(std::max(type.inputs.size(), type.outputs.size()));
}

PointF portPoint(const scheme::Block& block, int port, bool output)
{
  return {block.x + (output ? blockWidth : 0), block.y + 50 + 28 * port};
}

PointF world(PointF point)
{
  return {(point.X - canvasLeft() - editor.pan.X) / editor.zoom,
          (point.Y - headerHeight - editor.pan.Y) / editor.zoom};
}

PointF screen(PointF point)
{
  return {canvasLeft() + editor.pan.X + point.X * editor.zoom,
          headerHeight + editor.pan.Y + point.Y * editor.zoom};
}

bool onCanvas(PointF point)
{
  return point.X > canvasLeft() && point.Y > headerHeight && point.Y < editor.height - footerHeight;
}

void refresh(bool changed = false)
{
  editor.dirty = editor.dirty || changed;
  const std::wstring title =
      (editor.dirty ? L"* " : L"") +
      (editor.path.empty() ? std::wstring(L"Untitled")
                           : std::filesystem::path(editor.path).filename().wstring()) +
      L" - Block Scheme";
  SetWindowTextW(editor.window, title.c_str());
  InvalidateRect(editor.window, nullptr, FALSE);
}

void text(Graphics& graphics, const std::wstring& value, float x, float y, float width,
          float height, float size, Color color, bool bold = false, bool right = false)
{
  FontFamily family(L"Segoe UI");
  Font font(&family, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
  SolidBrush brush(color);
  StringFormat format;
  format.SetLineAlignment(StringAlignmentCenter);
  format.SetAlignment(right ? StringAlignmentFar : StringAlignmentNear);
  format.SetTrimming(StringTrimmingEllipsisCharacter);
  format.SetFormatFlags(StringFormatFlagsNoWrap);
  graphics.DrawString(value.c_str(), -1, &font, RectF(x, y, width, height), &format, &brush);
}

Color accent(int type)
{
  static const Color colors[] = {Color(28, 139, 111), Color(47, 112, 196), Color(185, 109, 33),
                                 Color(185, 109, 33), Color(47, 112, 196), Color(193, 70, 84)};
  return colors[type];
}

std::array<PointF, 4> wirePath(PointF start, PointF end)
{
  const float middleX = (start.X + end.X) / 2;
  return {start, PointF(middleX, start.Y), PointF(middleX, end.Y), end};
}

void wire(Graphics& graphics, PointF start, PointF end, Color color, float width)
{
  const auto path = wirePath(start, end);
  Pen pen(color, width);
  graphics.DrawLines(&pen, path.data(), static_cast<INT>(path.size()));
}

void render(Graphics& graphics)
{
  graphics.SetSmoothingMode(SmoothingModeAntiAlias);
  graphics.Clear(Color(245, 247, 249));
  SolidBrush white(Color(255, 255, 255));
  SolidBrush panel(Color(250, 251, 252));
  SolidBrush ink(Color(33, 43, 51));
  SolidBrush dot(Color(209, 216, 222));
  Pen divider(Color(222, 227, 231));
  graphics.FillRectangle(&white, 0.0f, 0.0f, editor.width, headerHeight);
  graphics.FillRectangle(&panel, 0.0f, headerHeight, canvasLeft(), editor.height - headerHeight);
  graphics.DrawLine(&divider, canvasLeft(), headerHeight, canvasLeft(),
                    editor.height - footerHeight);
  graphics.DrawLine(&divider, 0.0f, headerHeight, editor.width, headerHeight);
  text(graphics, L"Block Scheme", 24, 10, 220, 40, 23, Color(33, 43, 51), true);
  text(graphics,
       std::to_wstring(editor.graph.blocks().size()) + L" blocks   /   " +
           std::to_wstring(editor.graph.connections().size()) + L" connections",
       280, 10, std::max(0.0f, editor.width - 304), 40, 14, Color(97, 110, 122), false, true);
  const auto tab = paletteTab();
  graphics.FillRectangle(&white, tab);
  graphics.DrawRectangle(&divider, tab);
  text(graphics, editor.paletteVisible ? L"<" : L">", tab.X + 8, tab.Y, 16, tab.Height, 16,
       Color(67, 80, 91), true);
  if (editor.paletteVisible)
    text(graphics, L"BLOCK PALETTE", 20, 82, 170, 26, 12, Color(97, 110, 122), true);
  const auto& types = scheme::blockTypes();
  for (int index = 0; editor.paletteVisible && index < static_cast<int>(types.size()); ++index)
  {
    const float top = 122.0f + index * 70.0f;
    SolidBrush fill(editor.palette == index ? Color(225, 239, 249) : Color(255, 255, 255));
    SolidBrush color(accent(index));
    graphics.FillRectangle(&fill, 16.0f, top, paletteWidth - 32, 58.0f);
    graphics.DrawRectangle(&divider, 16.0f, top, paletteWidth - 32, 58.0f);
    graphics.FillRectangle(&color, 16.0f, top, 4.0f, 58.0f);
    graphics.FillRectangle(&color, 30.0f, top + 19, 20.0f, 20.0f);
    graphics.FillEllipse(&white, 26.0f, top + 26, 6.0f, 6.0f);
    graphics.FillEllipse(&white, 48.0f, top + 26, 6.0f, 6.0f);
    text(graphics, wide(types[index].name), 64, top + 4, 132, 27, 15, Color(33, 43, 51), true);
    text(graphics,
         std::to_wstring(types[index].inputs.size()) + L" in   /   " +
             std::to_wstring(types[index].outputs.size()) + L" out",
         64, top + 29, 130, 20, 12, Color(97, 110, 122));
  }

  const auto state = graphics.Save();
  graphics.SetClip(RectF(canvasLeft() + 1, headerHeight + 1,
                         std::max(0.0f, editor.width - canvasLeft() - 1),
                         std::max(0.0f, editor.height - headerHeight - footerHeight - 1)));
  const float spacing = 24 * editor.zoom;
  const float gridX = canvasLeft() + std::fmod(editor.pan.X, spacing);
  const float gridY = headerHeight + std::fmod(editor.pan.Y, spacing);
  for (float horizontal = gridX; horizontal < editor.width; horizontal += spacing)
  {
    for (float vertical = gridY; vertical < editor.height - footerHeight; vertical += spacing)
    {
      graphics.FillEllipse(&dot, horizontal, vertical, 2.0f, 2.0f);
    }
  }
  graphics.TranslateTransform(canvasLeft() + editor.pan.X, headerHeight + editor.pan.Y);
  graphics.ScaleTransform(editor.zoom, editor.zoom);
  int linkIndex = 0;
  for (const auto& link : editor.graph.connections())
  {
    const auto* source = editor.graph.findBlock(link.source);
    const auto* target = editor.graph.findBlock(link.target);
    wire(graphics, portPoint(*source, link.output, true), portPoint(*target, link.input, false),
         linkIndex == editor.selectedWire ? Color(20, 136, 190) : Color(116, 135, 151),
         (linkIndex == editor.selectedWire ? 3.5f : 2.2f) / editor.zoom);
    ++linkIndex;
  }
  for (const auto& block : editor.graph.blocks())
  {
    const float height = blockHeight(block);
    const auto& type = types[block.type];
    SolidBrush shadow(Color(20, 20, 30, 40));
    SolidBrush title(accent(block.type));
    Pen border(block.id == editor.selected ? Color(20, 136, 190) : Color(198, 209, 218),
               (block.id == editor.selected ? 2.5f : 1.0f) / editor.zoom);
    graphics.FillRectangle(&shadow, block.x + 3, block.y + 4, blockWidth, height);
    graphics.FillRectangle(&white, block.x, block.y, blockWidth, height);
    graphics.FillRectangle(&title, block.x, block.y, blockWidth, 32.0f);
    graphics.DrawRectangle(&border, block.x, block.y, blockWidth, height);
    text(graphics, wide(type.name), block.x + 12, block.y, 125, 32, 14, Color(255, 255, 255), true);
    text(graphics, std::to_wstring(block.id), block.x + 136, block.y, 25, 32, 11,
         Color(255, 255, 255), false, true);
    for (int direction = 0; direction < 2; ++direction)
    {
      const auto& ports = direction == 1 ? type.outputs : type.inputs;
      for (int port = 0; port < static_cast<int>(ports.size()); ++port)
      {
        const auto point = portPoint(block, port, direction == 1);
        const bool active =
            editor.pending.block == block.id && editor.pending.port == port && direction == 1;
        SolidBrush fill(active ? Color(20, 136, 190) : Color(255, 255, 255));
        Pen outline(accent(block.type), 2.0f);
        graphics.FillEllipse(&fill, point.X - 5, point.Y - 5, 10.0f, 10.0f);
        graphics.DrawEllipse(&outline, point.X - 5, point.Y - 5, 10.0f, 10.0f);
        text(graphics, wide(ports[port]), block.x + 13, point.Y - 11, blockWidth - 26, 22, 12,
             Color(67, 80, 91), false, direction == 1);
      }
    }
  }
  if (editor.pending.block != -1)
  {
    const auto* block = editor.graph.findBlock(editor.pending.block);
    if (block)
      wire(graphics, portPoint(*block, editor.pending.port, true), world(editor.mouse),
           Color(20, 136, 190), 2.0f / editor.zoom);
  }
  graphics.Restore(state);
  graphics.FillRectangle(&white, 0.0f, editor.height - footerHeight, editor.width, footerHeight);
  graphics.DrawLine(&divider, 0.0f, editor.height - footerHeight, editor.width,
                    editor.height - footerHeight);
  text(graphics, editor.status, 16, editor.height - footerHeight,
       std::max(0.0f, editor.width - 112), footerHeight, 12, Color(80, 95, 107));
  text(graphics, std::to_wstring(static_cast<int>(editor.zoom * 100)) + L"%", editor.width - 80,
       editor.height - footerHeight, 64, footerHeight, 12, Color(80, 95, 107), false, true);
}

PortHit hitPort(PointF point)
{
  point = world(point);
  const float radius = 11 / editor.zoom;
  for (auto block = editor.graph.blocks().rbegin(); block != editor.graph.blocks().rend(); ++block)
  {
    const auto& type = scheme::blockTypes()[block->type];
    for (int direction = 0; direction < 2; ++direction)
    {
      const auto count = direction == 1 ? type.outputs.size() : type.inputs.size();
      for (int port = 0; port < static_cast<int>(count); ++port)
      {
        const auto position = portPoint(*block, port, direction == 1);
        if (std::hypot(point.X - position.X, point.Y - position.Y) <= radius)
        {
          return {block->id, port, direction == 1};
        }
      }
    }
  }
  return {};
}

int hitBlock(PointF point)
{
  point = world(point);
  for (auto block = editor.graph.blocks().rbegin(); block != editor.graph.blocks().rend(); ++block)
  {
    if (point.X >= block->x && point.X <= block->x + blockWidth && point.Y >= block->y &&
        point.Y <= block->y + blockHeight(*block))
      return block->id;
  }
  return -1;
}

int hitWire(PointF point)
{
  point = world(point);
  int index = 0;
  for (const auto& link : editor.graph.connections())
  {
    const auto start = portPoint(*editor.graph.findBlock(link.source), link.output, true);
    const auto end = portPoint(*editor.graph.findBlock(link.target), link.input, false);
    const auto path = wirePath(start, end);
    for (size_t segment = 1; segment < path.size(); ++segment)
    {
      const auto previous = path[segment - 1];
      const auto sample = path[segment];
      const float deltaX = sample.X - previous.X;
      const float deltaY = sample.Y - previous.Y;
      const float length = deltaX * deltaX + deltaY * deltaY;
      const float along =
          length > 0
              ? std::clamp(
                    ((point.X - previous.X) * deltaX + (point.Y - previous.Y) * deltaY) / length,
                    0.0f, 1.0f)
              : 0;
      if (std::hypot(point.X - previous.X - along * deltaX, point.Y - previous.Y - along * deltaY) <
          7 / editor.zoom)
        return index;
    }
    ++index;
  }
  return -1;
}

void cancelGesture()
{
  editor.pending = {};
  editor.dragging = -1;
  editor.paletteDrag = false;
  editor.panning = false;
  if (GetCapture() == editor.window)
    ReleaseCapture();
}

void finishConnection(PortHit hit)
{
  const bool connected =
      editor.graph.connect(editor.pending.block, editor.pending.port, hit.block, hit.port);
  editor.status = connected ? L"Connected" : L"Connection rejected: input occupied or same block";
  editor.pending = {};
  refresh(connected);
}

void leftDown(PointF point)
{
  editor.mouse = point;
  if (paletteTab().Contains(point))
  {
    cancelGesture();
    editor.palette = -1;
    editor.paletteVisible = !editor.paletteVisible;
    editor.status = L"Ready";
    refresh();
    return;
  }
  if (editor.paletteVisible && point.X < paletteWidth && point.Y >= 122)
  {
    const int type = static_cast<int>((point.Y - 122) / 70);
    if (type < static_cast<int>(scheme::blockTypes().size()) &&
        std::fmod(point.Y - 122, 70.0f) < 58)
    {
      cancelGesture();
      editor.palette = type;
      editor.paletteDrag = true;
      editor.status = wide(scheme::blockTypes()[type].name) + L" selected";
      SetCapture(editor.window);
      refresh();
    }
    return;
  }
  if (!onCanvas(point))
    return;
  const auto hit = hitPort(point);
  if (hit.block != -1)
  {
    editor.palette = -1;
    if (hit.output)
    {
      editor.pending = hit;
      editor.status = L"Connecting";
      SetCapture(editor.window);
    }
    else if (editor.pending.block != -1)
    {
      finishConnection(hit);
    }
    refresh();
    return;
  }
  editor.pending = {};
  if (editor.palette != -1)
  {
    const auto position = world(point);
    editor.selected = editor.graph.addBlock(editor.palette, position.X, position.Y);
    editor.selectedWire = -1;
    editor.palette = -1;
    editor.status = L"Block placed";
    refresh(true);
    return;
  }
  editor.selected = hitBlock(point);
  editor.selectedWire = editor.selected == -1 ? hitWire(point) : -1;
  if (editor.selected != -1)
  {
    const auto* block = editor.graph.findBlock(editor.selected);
    const auto position = world(point);
    editor.dragOffset = {position.X - block->x, position.Y - block->y};
    editor.dragging = editor.selected;
    SetCapture(editor.window);
  }
  editor.status = editor.selected != -1       ? L"Block selected"
                  : editor.selectedWire != -1 ? L"Connection selected"
                                              : L"Ready";
  refresh();
}

void leftUp(PointF point)
{
  if (editor.paletteDrag && onCanvas(point))
  {
    const auto position = world(point);
    editor.selected = editor.graph.addBlock(editor.palette, position.X, position.Y);
    editor.selectedWire = -1;
    editor.palette = -1;
    editor.status = L"Block placed";
    refresh(true);
  }
  if (editor.pending.block != -1 && onCanvas(point))
  {
    const auto hit = hitPort(point);
    if (hit.block != -1 && !hit.output)
      finishConnection(hit);
  }
  editor.dragging = -1;
  editor.paletteDrag = false;
  if (GetCapture() == editor.window)
    ReleaseCapture();
  refresh();
}

void moveMouse(PointF point)
{
  if (editor.panning)
  {
    editor.pan.X += point.X - editor.mouse.X;
    editor.pan.Y += point.Y - editor.mouse.Y;
  }
  else if (editor.dragging != -1)
  {
    if (auto* block = editor.graph.findBlock(editor.dragging))
    {
      const auto position = world(point);
      const float nextX = position.X - editor.dragOffset.X;
      const float nextY = position.Y - editor.dragOffset.Y;
      if (nextX != block->x || nextY != block->y)
      {
        block->x = nextX;
        block->y = nextY;
        editor.dirty = true;
      }
    }
  }
  editor.mouse = point;
  if (editor.panning || editor.dragging != -1 || editor.pending.block != -1)
    refresh();
}

void zoomAt(PointF point, float factor)
{
  const auto anchor = world(point);
  editor.zoom = std::clamp(editor.zoom * factor, 0.35f, 2.5f);
  editor.pan = {point.X - canvasLeft() - anchor.X * editor.zoom,
                point.Y - headerHeight - anchor.Y * editor.zoom};
  refresh();
}

void fit()
{
  if (editor.graph.blocks().empty())
  {
    editor.zoom = 1;
    editor.pan = {36, 40};
  }
  else
  {
    float left = editor.graph.blocks().front().x;
    float top = editor.graph.blocks().front().y;
    float right = left + blockWidth;
    float bottom = top;
    for (const auto& block : editor.graph.blocks())
    {
      left = std::min(left, block.x);
      top = std::min(top, block.y);
      right = std::max(right, block.x + blockWidth);
      bottom = std::max(bottom, block.y + blockHeight(block));
    }
    const float availableWidth = editor.width - canvasLeft() - 72;
    const float availableHeight = editor.height - headerHeight - footerHeight - 72;
    editor.zoom = std::clamp(
        std::min(availableWidth / (right - left), availableHeight / (bottom - top)), 0.35f, 1.5f);
    editor.pan = {
        (editor.width - canvasLeft() - (right - left) * editor.zoom) / 2 - left * editor.zoom,
        (editor.height - headerHeight - footerHeight - (bottom - top) * editor.zoom) / 2 -
            top * editor.zoom};
  }
  refresh();
}

bool choosePath(bool saving, std::wstring& path)
{
  wchar_t buffer[32768]{};
  if (saving && !path.empty())
    wcsncpy_s(buffer, path.c_str(), _TRUNCATE);
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = editor.window;
  dialog.lpstrFilter = L"Block Scheme (*.scheme)\0*.scheme\0All files\0*.*\0";
  dialog.lpstrFile = buffer;
  dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
  dialog.lpstrDefExt = L"scheme";
  dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                 (saving ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  if (!(saving ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)))
    return false;
  path = buffer;
  return true;
}

bool save(bool saveAs = false)
{
  std::wstring path = editor.path;
  if ((saveAs || path.empty()) && !choosePath(true, path))
    return false;
  if (!editor.graph.save(path))
  {
    MessageBoxW(editor.window, L"The scheme could not be saved.", L"Save failed",
                MB_OK | MB_ICONERROR);
    return false;
  }
  editor.path = path;
  editor.dirty = false;
  editor.status = L"Scheme saved";
  refresh();
  return true;
}

bool confirmDiscard()
{
  if (!editor.dirty)
    return true;
  const int answer = MessageBoxW(editor.window, L"Save changes to this scheme?", L"Unsaved changes",
                                 MB_YESNOCANCEL | MB_ICONQUESTION);
  return answer == IDNO || (answer == IDYES && save());
}

void command(int id)
{
  cancelGesture();
  switch (id)
  {
    case commandNew:
      if (confirmDiscard())
      {
        editor.graph = {};
        editor.path.clear();
        editor.dirty = false;
        editor.selected = editor.selectedWire = editor.palette = -1;
        editor.status = L"Ready";
        fit();
      }
      break;
    case commandOpen: {
      if (!confirmDiscard())
        break;
      std::wstring path;
      if (!choosePath(false, path))
        break;
      if (!editor.graph.load(path))
      {
        MessageBoxW(editor.window, L"This file is not a valid Block Scheme document.",
                    L"Open failed", MB_OK | MB_ICONERROR);
        break;
      }
      editor.path = path;
      editor.dirty = false;
      editor.selected = editor.selectedWire = editor.palette = -1;
      editor.status = L"Scheme opened";
      fit();
      break;
    }
    case commandSave:
      save();
      break;
    case commandSaveAs:
      save(true);
      break;
    case commandDelete: {
      bool changed = false;
      if (editor.selected != -1)
        changed = editor.graph.removeBlock(editor.selected);
      else if (editor.selectedWire != -1)
        changed = editor.graph.removeConnection(editor.selectedWire);
      editor.selected = editor.selectedWire = -1;
      editor.status = changed ? L"Selection deleted" : L"Nothing selected";
      refresh(changed);
      break;
    }
    case commandFit:
      fit();
      break;
    case commandExit:
      SendMessageW(editor.window, WM_CLOSE, 0, 0);
      break;
  }
  refresh();
}

PointF messagePoint(LPARAM parameter)
{
  return {GET_X_LPARAM(parameter) / editor.dpi, GET_Y_LPARAM(parameter) / editor.dpi};
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM parameter, LPARAM data)
{
  switch (message)
  {
    case WM_CREATE:
      editor.window = window;
      return 0;
    case WM_SIZE:
      editor.width = LOWORD(data) / editor.dpi;
      editor.height = HIWORD(data) / editor.dpi;
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    case WM_GETMINMAXINFO: {
      auto* limits = reinterpret_cast<MINMAXINFO*>(data);
      limits->ptMinTrackSize = {static_cast<LONG>(740 * editor.dpi),
                                static_cast<LONG>(660 * editor.dpi)};
      return 0;
    }
    case WM_DPICHANGED: {
      cancelGesture();
      editor.dpi = HIWORD(parameter) / 96.0f;
      const auto* rectangle = reinterpret_cast<RECT*>(data);
      SetWindowPos(window, nullptr, rectangle->left, rectangle->top,
                   rectangle->right - rectangle->left, rectangle->bottom - rectangle->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      const auto device = BeginPaint(window, &paint);
      RECT rectangle{};
      GetClientRect(window, &rectangle);
      if (rectangle.right > 0 && rectangle.bottom > 0)
      {
        Bitmap buffer(rectangle.right, rectangle.bottom, PixelFormat32bppARGB);
        Graphics offscreen(&buffer);
        offscreen.ScaleTransform(editor.dpi, editor.dpi);
        render(offscreen);
        Graphics onscreen(device);
        onscreen.DrawImage(&buffer, 0, 0);
      }
      EndPaint(window, &paint);
      return 0;
    }
    case WM_LBUTTONDOWN:
      SetFocus(window);
      leftDown(messagePoint(data));
      return 0;
    case WM_LBUTTONUP:
      leftUp(messagePoint(data));
      return 0;
    case WM_MOUSEMOVE:
      moveMouse(messagePoint(data));
      return 0;
    case WM_MBUTTONDOWN:
      cancelGesture();
      if (onCanvas(messagePoint(data)))
      {
        editor.panning = true;
        editor.mouse = messagePoint(data);
        SetCapture(window);
      }
      return 0;
    case WM_MBUTTONUP:
      editor.panning = false;
      ReleaseCapture();
      return 0;
    case WM_CAPTURECHANGED:
      editor.dragging = -1;
      editor.paletteDrag = false;
      editor.panning = false;
      return 0;
    case WM_CANCELMODE:
      cancelGesture();
      refresh();
      return 0;
    case WM_KILLFOCUS:
      cancelGesture();
      refresh();
      return 0;
    case WM_MOUSEWHEEL: {
      POINT position{GET_X_LPARAM(data), GET_Y_LPARAM(data)};
      ScreenToClient(window, &position);
      const PointF point(position.x / editor.dpi, position.y / editor.dpi);
      if (onCanvas(point))
        zoomAt(point, std::pow(1.12f, GET_WHEEL_DELTA_WPARAM(parameter) / 120.0f));
      return 0;
    }
    case WM_CONTEXTMENU: {
      cancelGesture();
      POINT position{GET_X_LPARAM(data), GET_Y_LPARAM(data)};
      if (position.x == -1 && position.y == -1)
      {
        position = {static_cast<LONG>(editor.width * editor.dpi / 2),
                    static_cast<LONG>(editor.height * editor.dpi / 2)};
        ClientToScreen(window, &position);
      }
      else
      {
        POINT client = position;
        ScreenToClient(window, &client);
        PointF point(client.x / editor.dpi, client.y / editor.dpi);
        editor.selected = onCanvas(point) ? hitBlock(point) : -1;
        editor.selectedWire = onCanvas(point) && editor.selected == -1 ? hitWire(point) : -1;
      }
      refresh();
      HMENU menu = CreatePopupMenu();
      AppendMenuW(menu,
                  MF_STRING | (editor.selected == -1 && editor.selectedWire == -1 ? MF_GRAYED : 0),
                  commandDelete, L"Delete");
      AppendMenuW(menu, MF_STRING, commandFit, L"Fit scheme");
      const int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, position.x,
                                        position.y, 0, window, nullptr);
      DestroyMenu(menu);
      if (chosen)
        command(chosen);
      return 0;
    }
    case WM_COMMAND:
      command(LOWORD(parameter));
      return 0;
    case WM_KEYDOWN:
      if (parameter == VK_ESCAPE)
      {
        cancelGesture();
        editor.palette = -1;
        editor.status = L"Ready";
        refresh();
      }
      else if (parameter == VK_DELETE)
        command(commandDelete);
      else if (parameter == VK_HOME)
        command(commandFit);
      else if (GetKeyState(VK_CONTROL) & 0x8000)
      {
        if (parameter == 'S')
          command(GetKeyState(VK_SHIFT) & 0x8000 ? commandSaveAs : commandSave);
        else if (parameter == 'O')
          command(commandOpen);
        else if (parameter == 'N')
          command(commandNew);
        else if (parameter == VK_OEM_PLUS || parameter == VK_ADD)
          zoomAt({(editor.width + canvasLeft()) / 2, editor.height / 2}, 1.12f);
        else if (parameter == VK_OEM_MINUS || parameter == VK_SUBTRACT)
          zoomAt({(editor.width + canvasLeft()) / 2, editor.height / 2}, 1 / 1.12f);
      }
      return 0;
    case WM_CLOSE:
      if (confirmDiscard())
        DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(window, message, parameter, data);
}

HMENU createMenu()
{
  HMENU menu = CreateMenu();
  HMENU file = CreatePopupMenu();
  AppendMenuW(file, MF_STRING, commandNew, L"&New\tCtrl+N");
  AppendMenuW(file, MF_STRING, commandOpen, L"&Open...\tCtrl+O");
  AppendMenuW(file, MF_STRING, commandSave, L"&Save\tCtrl+S");
  AppendMenuW(file, MF_STRING, commandSaveAs, L"Save &As...\tCtrl+Shift+S");
  AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(file, MF_STRING, commandExit, L"E&xit");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
  HMENU edit = CreatePopupMenu();
  AppendMenuW(edit, MF_STRING, commandDelete, L"&Delete selection\tDel");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"&Edit");
  HMENU view = CreatePopupMenu();
  AppendMenuW(view, MF_STRING, commandFit, L"&Fit scheme\tHome");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"&View");
  return menu;
}

bool smokeTest()
{
  const auto send = [](UINT message, PointF point) {
    SendMessageW(
        editor.window, message, message == WM_LBUTTONDOWN ? MK_LBUTTON : 0,
        MAKELPARAM(static_cast<int>(point.X * editor.dpi), static_cast<int>(point.Y * editor.dpi)));
  };
  const auto click = [&](PointF point) {
    send(WM_LBUTTONDOWN, point);
    send(WM_LBUTTONUP, point);
  };
  click({70, 145});
  click({paletteWidth - paletteTabWidth / 2, 96});
  if (editor.paletteVisible || editor.palette != -1 || editor.dirty ||
      !editor.graph.blocks().empty() || onCanvas({14, 145}) || !onCanvas({70, 145}))
    return false;
  click({paletteTabWidth / 2, 96});
  if (!editor.paletteVisible || onCanvas({70, 145}) || editor.dirty)
    return false;
  click({70, 145});
  click(screen({40, 80}));
  send(WM_LBUTTONDOWN, {70, 495});
  send(WM_MOUSEMOVE, screen({420, 80}));
  send(WM_LBUTTONUP, screen({420, 80}));
  if (editor.graph.blocks().size() != 2)
    return false;
  click({paletteWidth - paletteTabWidth / 2, 96});
  if (editor.paletteVisible)
    return false;
  const int source = editor.graph.blocks()[0].id;
  const int sink = editor.graph.blocks()[1].id;
  click(screen(portPoint(*editor.graph.findBlock(source), 0, true)));
  click(screen(portPoint(*editor.graph.findBlock(sink), 0, false)));
  if (editor.graph.connections().size() != 1)
    return false;
  send(WM_LBUTTONDOWN, screen({460, 92}));
  send(WM_MOUSEMOVE, screen({500, 132}));
  send(WM_LBUTTONUP, screen({500, 132}));
  if (std::abs(editor.graph.findBlock(sink)->x - 460) > 2)
    return false;
  const auto hiddenAnchor = world({700, 380});
  zoomAt({700, 380}, 1.12f);
  const auto hiddenZoomedAnchor = world({700, 380});
  if (std::hypot(hiddenAnchor.X - hiddenZoomedAnchor.X, hiddenAnchor.Y - hiddenZoomedAnchor.Y) >
      0.01f)
    return false;
  editor.zoom = 1;
  editor.pan = {36, 40};
  click({paletteTabWidth / 2, 96});
  if (!editor.paletteVisible || editor.graph.blocks().size() != 2)
    return false;
  click({70, 285});
  click(screen({260, 260}));
  if (editor.graph.blocks().size() != 3)
    return false;
  const int add = editor.graph.blocks().back().id;
  for (int input = 0; input < 2; ++input)
  {
    send(WM_LBUTTONDOWN, screen(portPoint(*editor.graph.findBlock(source), 0, true)));
    send(WM_LBUTTONUP, screen(portPoint(*editor.graph.findBlock(add), input, false)));
  }
  if (editor.graph.connections().size() != 3)
    return false;
  const auto anchor = world({700, 380});
  zoomAt({700, 380}, 1.12f);
  const auto zoomedAnchor = world({700, 380});
  if (std::hypot(anchor.X - zoomedAnchor.X, anchor.Y - zoomedAnchor.Y) > 0.01f)
    return false;
  const auto beforePan = editor.pan;
  send(WM_MBUTTONDOWN, {900, 500});
  send(WM_MOUSEMOVE, {920, 510});
  send(WM_MBUTTONUP, {920, 510});
  if (std::abs(editor.pan.X - beforePan.X - 20) > 2)
    return false;
  editor.zoom = 1;
  editor.pan = {36, 40};
  const auto wireStart = portPoint(*editor.graph.findBlock(source), 0, true);
  const auto wireEnd = portPoint(*editor.graph.findBlock(sink), 0, false);
  const auto connectionPath = wirePath(wireStart, wireEnd);
  for (size_t segment = 1; segment < connectionPath.size(); ++segment)
  {
    const auto start = connectionPath[segment - 1];
    const auto end = connectionPath[segment];
    if (start.X != end.X && start.Y != end.Y)
      return false;
    click(screen({(start.X + end.X) / 2, (start.Y + end.Y) / 2}));
    if (editor.selectedWire != 0)
      return false;
  }
  click(screen({(wireStart.X + wireEnd.X) / 2, (wireStart.Y + wireEnd.Y) / 2}));
  if (editor.selectedWire != 0)
    return false;
  SendMessageW(editor.window, WM_KEYDOWN, VK_DELETE, 0);
  if (editor.graph.connections().size() != 2)
    return false;
  click(screen(portPoint(*editor.graph.findBlock(source), 0, true)));
  click(screen(portPoint(*editor.graph.findBlock(sink), 0, false)));
  if (editor.graph.connections().size() != 3)
    return false;
  UpdateWindow(editor.window);
  bool rendered = false;
  {
    Bitmap snapshot(static_cast<INT>(editor.width), static_cast<INT>(editor.height),
                    PixelFormat32bppARGB);
    {
      Graphics graphics(&snapshot);
      render(graphics);
    }
    Color pixel;
    snapshot.GetPixel(static_cast<INT>(screen({50, 90}).X), static_cast<INT>(screen({50, 90}).Y),
                      &pixel);
    rendered = pixel.GetG() > pixel.GetR() && pixel.GetR() < 80;
    UINT size = 0;
    UINT count = 0;
    GetImageEncodersSize(&count, &size);
    std::vector<BYTE> storage(size);
    auto* codecs = reinterpret_cast<ImageCodecInfo*>(storage.data());
    GetImageEncoders(count, size, codecs);
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    const auto path = std::filesystem::path(executable).parent_path() / L"BlockScheme-smoke.png";
    for (UINT index = 0; index < count; ++index)
    {
      if (wcscmp(codecs[index].MimeType, L"image/png") == 0)
      {
        rendered = rendered && snapshot.Save(path.c_str(), &codecs[index].Clsid) == Ok;
        break;
      }
    }
  }
  send(WM_LBUTTONDOWN, screen(portPoint(*editor.graph.findBlock(source), 0, true)));
  send(WM_LBUTTONUP, screen(portPoint(*editor.graph.findBlock(sink), 0, false)));
  if (editor.graph.connections().size() != 3)
    return false;
  click(screen({500, 132}));
  SendMessageW(editor.window, WM_KEYDOWN, VK_DELETE, 0);
  if (editor.graph.blocks().size() != 2 || editor.graph.connections().size() != 2)
    return false;
  click(screen({80, 92}));
  SendMessageW(editor.window, WM_KEYDOWN, VK_DELETE, 0);
  return rendered && editor.graph.blocks().size() == 1 && editor.graph.connections().empty();
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int show)
{
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  editor.dpi = GetDpiForSystem() / 96.0f;
  ULONG_PTR token = 0;
  GdiplusStartupInput startup;
  if (GdiplusStartup(&token, &startup, nullptr) != Ok)
    return 1;
  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = windowProcedure;
  windowClass.hInstance = instance;
  windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  windowClass.lpszClassName = L"BlockSchemeEditor";
  RegisterClassW(&windowClass);
  HWND window =
      CreateWindowExW(0, windowClass.lpszClassName, L"Untitled - Block Scheme", WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(1200 * editor.dpi),
                      static_cast<int>(800 * editor.dpi), nullptr, createMenu(), instance, nullptr);
  if (!window)
  {
    GdiplusShutdown(token);
    return 1;
  }
  const bool testing = std::wstring(arguments) == L"--smoke-test";
  ShowWindow(window, testing ? SW_SHOWNOACTIVATE : show);
  UpdateWindow(window);
  if (testing)
  {
    const bool passed = smokeTest();
    editor.dirty = false;
    DestroyWindow(window);
    GdiplusShutdown(token);
    return passed ? 0 : 2;
  }
  MSG message{};
  int result = 0;
  while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0)
  {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  GdiplusShutdown(token);
  return result == -1 ? 1 : static_cast<int>(message.wParam);
}