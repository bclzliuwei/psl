#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "scheme_model.h"

void check(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

int main()
{
  try
  {
    scheme::Graph graph;
    const int source = graph.addBlock(0, 10, 20);
    const int add = graph.addBlock(2, 200, 30);
    const int sink = graph.addBlock(5, 400, 40);
    check(graph.addBlock(99, 0, 0) == -1, "invalid type accepted");
    check(graph.connect(source, 0, add, 0), "valid connection rejected");
    check(graph.connect(source, 0, add, 1), "output fan-out rejected");
    check(!graph.connect(source, 0, add, 0), "occupied input accepted");
    check(!graph.connect(add, 0, add, 0), "self connection accepted");
    check(!graph.connect(source, 1, sink, 0), "invalid output accepted");
    check(!graph.connect(source, 0, sink, 1), "invalid input accepted");
    check(!graph.connect(999, 0, sink, 0), "missing block accepted");
    check(graph.connect(add, 0, sink, 0), "downstream connection rejected");
    check(graph.removeConnection(0), "connection removal failed");
    check(graph.connect(source, 0, add, 0), "freed input not reusable");
    const auto path = std::filesystem::temp_directory_path() / "blockscheme-model-test.scheme";
    check(graph.save(path.wstring()), "save failed");
    scheme::Graph loaded;
    check(loaded.load(path.wstring()), "load failed");
    check(loaded.blocks().size() == 3 && loaded.connections().size() == 3, "round-trip differs");
    check(loaded.findBlock(source)->x == 10, "coordinates not preserved");
    {
      std::ofstream stream(path);
      stream << "BLOCKSCHEME 1\n1 1\n1 0 0 0\n1 0 999 0\n";
    }
    check(!loaded.load(path.wstring()), "invalid file accepted");
    check(loaded.blocks().size() == 3, "failed load modified graph");
    std::filesystem::remove(path);
    check(graph.removeBlock(add), "block removal failed");
    check(graph.connections().empty(), "dangling connections after deletion");
    check(!graph.removeBlock(add), "missing block deletion accepted");
    std::cout << "All graph checks passed.\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}