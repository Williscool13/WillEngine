#include <vk_loader.h>

VkFilter extract_filter(fastgltf::Filter filter)
{
	switch (filter) {
		// nearest samplers
	case fastgltf::Filter::Nearest:
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::NearestMipMapLinear:
		return VK_FILTER_NEAREST;

		// linear samplers
	case fastgltf::Filter::Linear:
	case fastgltf::Filter::LinearMipMapNearest:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_FILTER_LINEAR;
	}
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
	switch (filter) {
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::LinearMipMapNearest:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;

	case fastgltf::Filter::NearestMipMapLinear:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
}

std::string getFileExtension(const std::string& filename) {
	size_t dotPos = filename.find_last_of(".");
	if (dotPos == std::string::npos) {
		return ""; // No extension found
	}
	return filename.substr(dotPos + 1);
}

bool isKTXFile(const std::string& extension) {
	return extension == "ktx" || extension == "ktx2";
}


std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, std::filesystem::path filepath)
{
	AllocatedImage newImage{};

	int width, height, nrChannels;

 	std::visit(
		fastgltf::visitor{ 
			[](auto& arg) {},
			[&](fastgltf::sources::URI& filePath) {
				fmt::print("Loading URI image\n");
				assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
				assert(filePath.uri.isLocalPath()); // We're only capable of loading
				// local files.
				const std::string path(filePath.uri.path().begin(), filePath.uri.path().end()); // Thanks C++.
				std::string fullpath = filepath.string() + "\\" + path;
				std::string extension = getFileExtension(fullpath);

				if (isKTXFile(extension)) {
					ktxTexture* kTexture;
					KTX_error_code ktxresult;

					ktxresult = ktxTexture_CreateFromNamedFile(
						fullpath.c_str(),
						KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
						&kTexture);
					

					

					if (ktxresult == KTX_SUCCESS) {
						VkImageFormatProperties formatProperties;
						VkResult result = vkGetPhysicalDeviceImageFormatProperties(engine->_physicalDevice
							, ktxTexture_GetVkFormat(kTexture), VK_IMAGE_TYPE_2D
							, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &formatProperties);
						if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
							fmt::print("Format not supported\n");
							VkExtent3D imagesize;
							imagesize.width = 1;
							imagesize.height = 1;
							imagesize.depth = 1;
							unsigned char data[4] = { 255, 0, 255, 1 };
							newImage = engine->create_image(data, 4, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
						}
						else {
							unsigned char* data = (unsigned char*)ktxTexture_GetData(kTexture);
							VkExtent3D imageExtents;
							imageExtents.width = kTexture->baseWidth;
							imageExtents.height = kTexture->baseHeight;
							imageExtents.depth = 1;
							newImage = engine->create_image(data, kTexture->dataSize, imageExtents, ktxTexture_GetVkFormat(kTexture), VK_IMAGE_USAGE_SAMPLED_BIT, false);
						}
						
					}

					ktxTexture_Destroy(kTexture);
				}
				else {
					unsigned char* data = stbi_load(fullpath.c_str(), &width, &height, &nrChannels, 4);
					if (data) {
						VkExtent3D imagesize;
						imagesize.width = width;
						imagesize.height = height;
						imagesize.depth = 1;
						size_t size = width * height * 4;
						newImage = engine->create_image(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);

						stbi_image_free(data);
					}
				}
				
				
			},
			[&](fastgltf::sources::Array& vector) {
				fmt::print("Loading Array image\n");
				unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
					&width, &height, &nrChannels, 4);
				if (data) {
					VkExtent3D imagesize;
					imagesize.width = width;
					imagesize.height = height;
					imagesize.depth = 1;
					size_t size = width * height * 4;
					newImage = engine->create_image(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

					stbi_image_free(data);
				}
			},
			[&](fastgltf::sources::BufferView& view) {
				fmt::print("Loading BufferView image\n");
				auto& bufferView = asset.bufferViews[view.bufferViewIndex];
				auto& buffer = asset.buffers[bufferView.bufferIndex];
				// We only care about VectorWithMime here, because we
				// specify LoadExternalBuffers, meaning all buffers
				// are already loaded into a vector.
				std::visit(
					fastgltf::visitor { 
						[](auto& arg) {},
						[&](fastgltf::sources::Array& vector) {
							unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
							if (data) {
								VkExtent3D imagesize;
								imagesize.width = width;
								imagesize.height = height;
								imagesize.depth = 1;
								size_t size = width * height * 4;
								newImage = engine->create_image(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
									VK_IMAGE_USAGE_SAMPLED_BIT,false);
								stbi_image_free(data);
							}
						} 
					} , buffer.data);
			},
	}, image.data);

	// if any of the attempts to load the data failed, we havent written the image
	// so handle is null
	if (newImage.image == VK_NULL_HANDLE) {
		fmt::print("Image failed to load: {}\n", image.name.c_str());
		return {};
	}
	else {
		return newImage;
	}
}


std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath)
{
	fmt::print("Loading GLTF: {}\n", filePath);

	std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
	scene->creator = engine;
	LoadedGLTF& file = *scene.get();

	fastgltf::Parser parser{};

	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
		| fastgltf::Options::AllowDouble
		//| fastgltf::Options::LoadGLBBuffers 
		| fastgltf::Options::LoadExternalBuffers;

	fastgltf::GltfDataBuffer data;
	data.FromPath(filePath);
	fastgltf::Asset gltf;

	auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);
	if (!bool(gltfFile)) { fmt::print("Failed to open glTF file: {}\n", fastgltf::getErrorMessage(gltfFile.error())); return {}; }

	std::filesystem::path path = filePath;
	auto load = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
	if (!load) {
		fmt::print("Failed to load glTF: {}\n", fastgltf::to_underlying(load.error()));
		return {};
	}

	gltf = std::move(load.get());

	// load samplers
	for (fastgltf::Sampler& sampler : gltf.samplers) {

		VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
		sampl.maxLod = VK_LOD_CLAMP_NONE;
		sampl.minLod = 0;

		sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
		sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

		sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

		VkSampler newSampler;
		vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);

		file.samplers.push_back(newSampler);
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<std::shared_ptr<Node>> nodes;
	std::vector<AllocatedImage> images;
	std::vector<std::shared_ptr<GLTFMaterial>> materials;

	// load all textures
	for (fastgltf::Image& image : gltf.images) {
		std::optional<AllocatedImage> img = load_image(engine, gltf, image, path.parent_path());

		if (img.has_value()) {
			images.push_back(*img);
			std::string base_name = image.name.c_str();
			std::string unique_name = base_name;
			int i = 0;
			while (file.images.find(unique_name) != file.images.end()) {
				unique_name = base_name + std::to_string(i);
				i++;
			}
			file.images[unique_name] = *img;
		}
		else {
			// we failed to load, so lets give the slot a default white texture to not
			// completely break loading
			images.push_back(engine->_errorCheckerboardImage);
		}
	}

	int data_index = 0;

	for (fastgltf::Material& mat : gltf.materials) {
		std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
		materials.push_back(newMat);
		file.materials[mat.name.c_str()] = newMat;

		GLTFMetallic_Roughness::MaterialConstants constants;
		constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
		constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
		constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
		constants.colorFactors.w = mat.pbrData.baseColorFactor[3];
		constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
		constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;


		AllocatedBuffer buffer = engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants)
			, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			, VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants =
			(GLTFMetallic_Roughness::MaterialConstants*)buffer.info.pMappedData;
		memcpy(sceneMaterialConstants, &constants, sizeof(GLTFMetallic_Roughness::MaterialConstants));


		// write material parameters to buffer - shouldnt this be memcpy?
		//sceneMaterialConstants[data_index] = constants;

		MaterialPass passType = MaterialPass::MainColor;
		GLTFMetallic_Roughness::MaterialResources materialResources;
		materialResources.alphaCutoff = 0.0f;
		if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
			passType = MaterialPass::Transparent;
			fmt::print("Transparent material found\n");
		} else if (mat.alphaMode == fastgltf::AlphaMode::Mask) {
			materialResources.alphaCutoff = mat.alphaCutoff;
			fmt::print("Masked material found\n");
		}

		// default the material textures
		materialResources.colorImage = engine->_whiteImage;
		materialResources.colorSampler = engine->_defaultSamplerLinear;
		materialResources.metalRoughImage = engine->_whiteImage;
		materialResources.metalRoughSampler = engine->_defaultSamplerLinear;

		// set the uniform buffer for the material data
		materialResources.dataBuffer = buffer;
		materialResources.dataBufferSize = sizeof(GLTFMetallic_Roughness::MaterialConstants);

		// grab textures from gltf file
		if (mat.pbrData.baseColorTexture.has_value()) {
			if (gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.has_value()) {
				size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				materialResources.colorImage = images[img];
			}
			if (gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.has_value()) {
				size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
				materialResources.colorSampler = file.samplers[sampler];
			}


		}
		// build material
		newMat->data = engine->metallicRoughnessPipelines.write_material(
			engine->_device, passType, materialResources);

		data_index++;
	}


	// Meshes
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	for (fastgltf::Mesh& mesh : gltf.meshes) {
		std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
		meshes.push_back(newmesh);
		file.meshes[mesh.name.c_str()] = newmesh;
		newmesh->name = mesh.name;

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives) {
			GeoSurface newSurface;
			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

			size_t initial_vtx = vertices.size();

			// load indexes
			{
				fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexaccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
					[&](std::uint32_t idx) {
						indices.push_back(idx + static_cast<uint32_t>(initial_vtx));
					});
			}

			// load vertex positions
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 v, size_t index) {
						Vertex newvtx;
						newvtx.position = v;
						newvtx.normal = { 1, 0, 0 };
						newvtx.color = glm::vec4{ 1.f };
						newvtx.uv_x = 0;
						newvtx.uv_y = 0;
						vertices[initial_vtx + index] = newvtx;
					});
			}

			// load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
					[&](glm::vec3 v, size_t index) {
						vertices[initial_vtx + index].normal = v;
					});
			}

			// load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
					[&](glm::vec2 v, size_t index) {
						vertices[initial_vtx + index].uv_x = v.x;
						vertices[initial_vtx + index].uv_y = v.y;
					});
			}

			// load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
					[&](glm::vec4 v, size_t index) {
						vertices[initial_vtx + index].color = v;
					});
			}

			if (p.materialIndex.has_value()) {
				newSurface.material = materials[p.materialIndex.value()];
			}
			else {
				newSurface.material = materials[0];
			}

			newmesh->surfaces.push_back(newSurface);
		}
		newmesh->meshBuffers = engine->uploadMesh(indices, vertices);
	}


	// Node Transformations/Init
	for (fastgltf::Node& node : gltf.nodes) {
		std::shared_ptr<Node> newNode;

		// find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
		if (node.meshIndex.has_value()) {
			newNode = std::make_shared<MeshNode>();
			static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
		}
		else {
			newNode = std::make_shared<Node>();
		}


		nodes.push_back(newNode);
		file.nodes[node.name.c_str()];

		std::visit(
			fastgltf::visitor{
				[&](fastgltf::math::fmat4x4 matrix) {
					memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
				},
				[&](fastgltf::TRS transform) {
					glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
					glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
					glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

					glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
					glm::mat4 rm = glm::toMat4(rot);
					glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

					newNode->localTransform = tm * rm * sm;
				}
			}
			, node.transform
					);
	}

	// Node Hierarchy
	for (int i = 0; i < gltf.nodes.size(); i++) {
		fastgltf::Node& node = gltf.nodes[i];
		std::shared_ptr<Node>& sceneNode = nodes[i];
		for (auto& c : node.children) {
			sceneNode->children.push_back(nodes[c]);
			nodes[c]->parent = sceneNode;
		}
	}

	// find the top nodes, with no parents
	for (auto& node : nodes) {
		if (node->parent.lock() == nullptr) {
			file.topNodes.push_back(node);
			node->refreshTransform(glm::mat4{ 1.f });
		}
	}

	return scene;
}

std::optional<std::shared_ptr<LoadedGLTFMultiDraw>> loadGltfMultiDraw(VulkanEngine* engine, std::string_view filePath)
{
	fmt::print("Loading GLTF: {}\n", filePath);

	std::shared_ptr<LoadedGLTFMultiDraw> scene = std::make_shared<LoadedGLTFMultiDraw>();
	scene->creator = engine;
	LoadedGLTFMultiDraw& file = *scene.get();

	fastgltf::Parser parser{};

	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
		| fastgltf::Options::AllowDouble
		//| fastgltf::Options::LoadGLBBuffers 
		| fastgltf::Options::LoadExternalBuffers;

	fastgltf::GltfDataBuffer data;
	data.FromPath(filePath);
	fastgltf::Asset gltf;

	auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);
	if (!bool(gltfFile)) { fmt::print("Failed to open glTF file: {}\n", fastgltf::getErrorMessage(gltfFile.error())); return {}; }

	std::filesystem::path path = filePath;
	auto load = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
	if (!load) { fmt::print("Failed to load glTF: {}\n", fastgltf::to_underlying(load.error())); return {}; }

	gltf = std::move(load.get());

	// load samplers
	for (fastgltf::Sampler& sampler : gltf.samplers) {

		VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
		sampl.maxLod = VK_LOD_CLAMP_NONE;
		sampl.minLod = 0;

		sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
		sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

		sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

		VkSampler newSampler;
		vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);

		file.samplers.push_back(newSampler);
	}

	// load all textures
	file.images.reserve(gltf.images.size());
	for (int i = 0; i < gltf.images.size(); i++) {
		fastgltf::Image& image = gltf.images[i];
		int j = 0;
		std::optional<AllocatedImage> img = load_image(engine, gltf, image, path.parent_path());

		if (img.has_value()) {
			file.images.emplace_back(*img);
			//file.images[i] = *img;
		}
		else {
			fmt::print("Failed to load iamge, this is not supported");
			abort();
		}
	}
	assert(file.images.size() == gltf.images.size());

	std::vector<MaterialPass> materialType;
	materialType.reserve(gltf.materials.size());
	for (int i=0;i<gltf.materials.size();i++) {
		MaterialData data;
		data.color_factor = glm::vec4(
			  gltf.materials[i].pbrData.baseColorFactor[0]
			, gltf.materials[i].pbrData.baseColorFactor[1]
			, gltf.materials[i].pbrData.baseColorFactor[2]
			, gltf.materials[i].pbrData.baseColorFactor[3]
		);
		data.metal_rough_factors.x = gltf.materials[i].pbrData.metallicFactor;
		data.metal_rough_factors.y = gltf.materials[i].pbrData.roughnessFactor;
		data.alphaCutoff = 0.0f;
		materialType.push_back(MaterialPass::MainColor);

		if (gltf.materials[i].alphaMode == fastgltf::AlphaMode::Blend) {
			materialType[i] = MaterialPass::Transparent;
		}
		else if (gltf.materials[i].alphaMode == fastgltf::AlphaMode::Mask) {
			data.alphaCutoff = gltf.materials[i].alphaCutoff;
		}

		// grab textures from gltf file
		if (gltf.materials[i].pbrData.baseColorTexture.has_value()) {
			size_t img = 0;
			size_t sam = 0;
			if (gltf.textures[gltf.materials[i].pbrData.baseColorTexture.value().textureIndex].imageIndex.has_value()) {
				img = gltf.textures[gltf.materials[i].pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				//materialResources.colorImage = images[img];
			}
			else {
				fmt::print("Texture has no image index\n");
				abort();
			}
			if (gltf.textures[gltf.materials[i].pbrData.baseColorTexture.value().textureIndex].samplerIndex.has_value()) {
				sam = gltf.textures[gltf.materials[i].pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
			}
			else {
				fmt::print("Texture has no sampler index\n");
				abort();
			}
			data.textureIndex1 = img;
			data.samplerIndex1 = sam;	
		}
		if (gltf.materials[i].pbrData.metallicRoughnessTexture.has_value()) {
			size_t img = 0;
			size_t sam = 0;
			if (gltf.textures[gltf.materials[i].pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.has_value()) {
				img = gltf.textures[gltf.materials[i].pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
				//materialResources.colorImage = images[img];
			}
			else {
				fmt::print("Metallic Texture has no image index\n");
				abort();
			}
			if (gltf.textures[gltf.materials[i].pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.has_value()) {
				sam = gltf.textures[gltf.materials[i].pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value();
			}
			else {
				fmt::print("Metallic Texture has no sampler index\n");
				abort();
			}
			data.textureIndex2 = img;
			data.samplerIndex2 = sam;
		}

		file.materials.push_back(data);
	}
	assert(file.materials.size() == gltf.materials.size());

	std::vector<MultiDrawVertex> vertices;
	std::vector<uint32_t> indices;
	for (fastgltf::Mesh& mesh : gltf.meshes) {

		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives) {
			size_t initial_vtx = vertices.size();

			// load indexes
			{
				fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexaccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
					[&](std::uint32_t idx) {
						indices.push_back(idx + static_cast<uint32_t>(initial_vtx));
					});
			}

			// load vertex positions
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 v, size_t index) {
						MultiDrawVertex newvtx;
						newvtx.position = v;
						newvtx.normal = { 1, 0, 0 };
						newvtx.color = glm::vec4{ 1.f };
						newvtx.uv_x = 0;
						newvtx.uv_y = 0;
						vertices[initial_vtx + index] = newvtx;
					});


				for (int i = initial_vtx; i < vertices.size(); i++) {
					if (p.materialIndex.has_value()) {
						vertices[i].materialIndex = p.materialIndex.value();
					}
					else {
						vertices[i].materialIndex = 0;
					}
				}
			}

			// load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
					[&](glm::vec3 v, size_t index) {
						vertices[initial_vtx + index].normal = v;
					});
			}

			// load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
					[&](glm::vec2 v, size_t index) {
						vertices[initial_vtx + index].uv_x = v.x;
						vertices[initial_vtx + index].uv_y = v.y;
					});
			}

			// load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
					[&](glm::vec4 v, size_t index) {
						vertices[initial_vtx + index].color = v;
					});
			}
		}
		file.primitives.push_back({ vertices, indices });

	}

	std::vector<std::shared_ptr<Node>> nodes;
	for (fastgltf::Node& node : gltf.nodes) {
		std::shared_ptr<Node> newNode;

		// find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
		if (node.meshIndex.has_value()) {
			newNode = std::make_shared<MeshNodeMultiDraw>();
			static_cast<MeshNodeMultiDraw*>(newNode.get())->meshIndex = *node.meshIndex;
		}
		else {
			newNode = std::make_shared<Node>();
		}


		nodes.push_back(newNode);
		file.nodes.push_back(newNode);

		std::visit(
			fastgltf::visitor{
				[&](fastgltf::math::fmat4x4 matrix) {
					memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
				},
				[&](fastgltf::TRS transform) {
					glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
					glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
					glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

					glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
					glm::mat4 rm = glm::toMat4(rot);
					glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

					newNode->localTransform = tm * rm * sm;
				}
			}
			, node.transform
					);
	}

	// Node Hierarchy
	for (int i = 0; i < gltf.nodes.size(); i++) {
		fastgltf::Node& node = gltf.nodes[i];
		std::shared_ptr<Node>& sceneNode = nodes[i];
		for (auto& c : node.children) {
			sceneNode->children.push_back(nodes[c]);
			nodes[c]->parent = sceneNode;
		}
	}

	// find the top nodes, with no parents
	for (auto& node : nodes) {
		if (node->parent.lock() == nullptr) {
			file.topNodes.push_back(node);
			node->refreshTransform(glm::mat4{ 1.f });
		}
	}

	return scene;
}

void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	// create renderables from the scenenodes
	for (auto& n : topNodes) {
		n->Draw(topMatrix, ctx);
	}
}

void LoadedGLTF::clearAll()
{
	VkDevice dv = creator->_device;

	for (auto& [k, v] : materials) {
		v->data.pipeline->materialTextureDescriptorBuffer->free_descriptor_buffer(v->data.textureDescriptorBufferIndex);
		v->data.pipeline->materialUniformDescriptorBuffer->free_descriptor_buffer(v->data.uniformDescriptorBufferIndex);
		creator->destroy_buffer(v->data.materialUniformBuffer);
	}

	for (auto& [k, v] : meshes) {

		creator->destroy_buffer(v->meshBuffers.indexBuffer);
		creator->destroy_buffer(v->meshBuffers.vertexBuffer);
	}

	for (auto& [k, v] : images) {

		if (v.image == creator->_errorCheckerboardImage.image) {
			//dont destroy the default images
			continue;
		}

		creator->destroy_image(v);
	}

	for (auto& sampler : samplers) {
		vkDestroySampler(dv, sampler, nullptr);
	}

}

void LoadedGLTFMultiDraw::clearAll()
{
	VkDevice dv = creator->_device;

	for (auto& image : images) {

		if (image.image == creator->_errorCheckerboardImage.image) {
			//dont destroy the default images
			continue;
		}

		creator->destroy_image(image);
	}

	for (auto& sampler : samplers) {
		vkDestroySampler(dv, sampler, nullptr);
	}

	fmt::print("Destroying Loaded GLTF Multi Draw Struct");
}
