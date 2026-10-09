#include "scheme_model.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace scheme {

const std::vector<BlockType>& blockTypes() {
    static const std::vector<BlockType> types = {
        {"Source", {}, {"Value"}},
        {"Gain", {"In"}, {"Out"}},
        {"Add", {"A", "B"}, {"Sum"}},
        {"Multiply", {"A", "B"}, {"Product"}},
        {"Split", {"In"}, {"Out 1", "Out 2"}},
        {"Sink", {"In"}, {}}
    };
    return types;
}

int Graph::addBlock(int type, float x, float y) {
    if (type < 0 || type >= static_cast<int>(blockTypes().size()) ||
        !std::isfinite(x) || !std::isfinite(y) || nextId_ == std::numeric_limits<int>::max()) {
        return -1;
    }
    const int id = nextId_++;
    blocks_.push_back({id, type, x, y});
    return id;
}

Block* Graph::findBlock(int id) {
    auto found = std::find_if(blocks_.begin(), blocks_.end(),
        [id](const Block& block) { return block.id == id; });
    return found == blocks_.end() ? nullptr : &*found;
}

const Block* Graph::findBlock(int id) const {
    auto found = std::find_if(blocks_.begin(), blocks_.end(),
        [id](const Block& block) { return block.id == id; });
    return found == blocks_.end() ? nullptr : &*found;
}

bool Graph::removeBlock(int id) {
    if (!findBlock(id)) return false;
    connections_.erase(std::remove_if(connections_.begin(), connections_.end(),
        [id](const Connection& link) { return link.source == id || link.target == id; }),
        connections_.end());
    blocks_.erase(std::remove_if(blocks_.begin(), blocks_.end(),
        [id](const Block& block) { return block.id == id; }), blocks_.end());
    return true;
}

bool Graph::connect(int source, int output, int target, int input) {
    const auto* sourceBlock = findBlock(source);
    const auto* targetBlock = findBlock(target);
    if (!sourceBlock || !targetBlock || source == target) return false;
    const auto& sourceType = blockTypes()[sourceBlock->type];
    const auto& targetType = blockTypes()[targetBlock->type];
    if (output < 0 || output >= static_cast<int>(sourceType.outputs.size()) ||
        input < 0 || input >= static_cast<int>(targetType.inputs.size())) return false;
    if (std::any_of(connections_.begin(), connections_.end(),
        [target, input](const Connection& link) { return link.target == target && link.input == input; })) {
        return false;
    }
    connections_.push_back({source, output, target, input});
    return true;
}

bool Graph::removeConnection(int index) {
    if (index < 0 || index >= static_cast<int>(connections_.size())) return false;
    connections_.erase(connections_.begin() + index);
    return true;
}

bool Graph::save(const std::wstring& path) const {
    std::ofstream stream{std::filesystem::path(path)};
    if (!stream) return false;
    stream.precision(std::numeric_limits<float>::max_digits10);
    stream << "BLOCKSCHEME 1\n" << blocks_.size() << ' ' << connections_.size() << '\n';
    for (const auto& block : blocks_) {
        stream << block.id << ' ' << block.type << ' ' << block.x << ' ' << block.y << '\n';
    }
    for (const auto& link : connections_) {
        stream << link.source << ' ' << link.output << ' ' << link.target << ' ' << link.input << '\n';
    }
    stream.flush();
    return static_cast<bool>(stream);
}

bool Graph::load(const std::wstring& path) {
    std::ifstream stream{std::filesystem::path(path)};
    std::string magic;
    int version = 0;
    int blockCount = 0;
    int connectionCount = 0;
    if (!(stream >> magic >> version >> blockCount >> connectionCount) ||
        magic != "BLOCKSCHEME" || version != 1 || blockCount < 0 || blockCount > 10000 ||
        connectionCount < 0 || connectionCount > 50000) return false;
    Graph candidate;
    for (int index = 0; index < blockCount; ++index) {
        Block block{};
        if (!(stream >> block.id >> block.type >> block.x >> block.y) || block.id < 1 ||
            block.id == std::numeric_limits<int>::max() || candidate.findBlock(block.id) ||
            block.type < 0 || block.type >= static_cast<int>(blockTypes().size()) ||
            !std::isfinite(block.x) || !std::isfinite(block.y)) return false;
        candidate.blocks_.push_back(block);
        candidate.nextId_ = std::max(candidate.nextId_, block.id + 1);
    }
    for (int index = 0; index < connectionCount; ++index) {
        Connection link{};
        if (!(stream >> link.source >> link.output >> link.target >> link.input) ||
            !candidate.connect(link.source, link.output, link.target, link.input)) return false;
    }
    stream >> std::ws;
    if (!stream.eof()) return false;
    *this = std::move(candidate);
    return true;
}

}