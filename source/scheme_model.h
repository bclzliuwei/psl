#pragma once

#include <string>
#include <vector>

namespace scheme
{

struct BlockType
{
  std::string name;
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
};

const std::vector<BlockType>& blockTypes();

struct Block
{
  int id;
  int type;
  float x;
  float y;
};

struct Connection
{
  int source;
  int output;
  int target;
  int input;
};

class Graph
{
 public:
  int addBlock(int type, float x, float y);
  bool removeBlock(int id);
  bool connect(int source, int output, int target, int input);
  bool removeConnection(int index);
  Block* findBlock(int id);
  const Block* findBlock(int id) const;
  const std::vector<Block>& blocks() const
  {
    return blocks_;
  }
  const std::vector<Connection>& connections() const
  {
    return connections_;
  }
  bool save(const std::wstring& path) const;
  bool load(const std::wstring& path);

 private:
  int nextId_ = 1;
  std::vector<Block> blocks_;
  std::vector<Connection> connections_;
};

}  // namespace scheme