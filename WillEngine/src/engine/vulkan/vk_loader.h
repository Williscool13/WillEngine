﻿#pragma once
#include "vk_types.h"
#include "will_engine.h"
#include "vk_engine.h"
// ktx
#include <ktxvulkan.h>
// implementation in vk_Engine.cpp
#include <stb_image/stb_image.h>
// only used here
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

class VulkanEngine;

struct LoadedGLTFMultiDraw {
    VulkanEngine* creator;
    // cosntruct buffers from these
    std::vector<AllocatedImage> images;
    std::vector<VkSampler> samplers;
    std::vector<MaterialData> materials; // materials use images/samplers

    std::vector<RawMeshData> meshes; // vertices use materials
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<std::shared_ptr<Node>> topNodes;

    ~LoadedGLTFMultiDraw() { clearAll(); };
private:
    void clearAll();
};


std::optional<std::shared_ptr<LoadedGLTFMultiDraw>> loadGltfMultiDraw(VulkanEngine* engine, std::string_view filePath);
