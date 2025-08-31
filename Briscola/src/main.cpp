// This has been adapted from the Vulkan tutorial
#include <sstream>

#include <json.hpp>

#include "modules/Starter.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"
#include "modules/Animations.hpp"

#include "gamecontroller.h"
#include "modules/CardAnimator.hpp"

// The uniform buffer object used in this example
struct VertexChar {
	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 UV;
	glm::uvec4 jointIndices;
	glm::vec4 weights;
};

struct VertexSimp {
	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 UV;
};

struct skyBoxVertex {
	glm::vec3 pos;
};

struct VertexTan {
	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 UV;
	glm::vec4 tan;
};

struct GlobalUniformBufferObject {
	alignas(16) glm::vec3 lightDir;
	alignas(16) glm::vec4 lightColor;
	alignas(16) glm::vec3 eyePos;
};

struct UniformBufferObjectChar {
	alignas(16) glm::vec4 debug1;
	alignas(16) glm::mat4 mvpMat[65];
	alignas(16) glm::mat4 mMat[65];
	alignas(16) glm::mat4 nMat[65];
};

struct UniformBufferObjectSimp {
	alignas(16) glm::mat4 mvpMat;
	alignas(16) glm::mat4 mMat;
	alignas(16) glm::mat4 nMat;
};

struct UniformBufferObjectCard {
	alignas(16) glm::mat4 mvpMat;
	alignas(16) glm::mat4 mMat;
	alignas(16) glm::mat4 nMat;
	alignas(4)  int cardIndex;
	alignas(4)  int highlightCardIndex;
	int _pad[2];
};

struct skyBoxUniformBufferObject {
	alignas(16) glm::mat4 mvpMat;
};

enum class GameState {
	MENU,
	PLAYING,
	GAME_OVER
};
	
// MAIN !
class BRISCOLA : public BaseProject {
	protected:
	// Here you list all the Vulkan objects you need:
	
	// Descriptor Layouts [what will be passed to the shaders]
	DescriptorSetLayout DSLlocalCard, DSLlocalChar, DSLlocalSimp, DSLlocalPBR, DSLglobal, DSLskyBox;

	// Vertex formants, Pipelines [Shader couples] and Render passes
	VertexDescriptor VDchar;
	VertexDescriptor VDsimp;
	VertexDescriptor VDskyBox;
	VertexDescriptor VDtan;

	RenderPass RP;
	Pipeline Pcard, Pchar, PsimpObj, PskyBox, P_PBR;

	//*DBG*/Pipeline PDebug;

	// Models, textures and Descriptors (values assigned to the uniforms)
	Scene SC;
	std::vector<VertexDescriptorRef>  VDRs;
	std::vector<TechniqueRef> PRs;
	//*DBG*/Model MS;
	//*DBG*/DescriptorSet SSD;

	// Briscola game and animation
	GameController gc;
	GameState gameState;
	std::vector<Card> playerCards;
	std::vector<Card> cpuCards;
	std::vector<Card> playerPile;
	std::vector<Card> cpuPile;
	std::unique_ptr<CardAnimator> ca;
	int cpuChoice;
	int cpuCardId;
	int menuIndex; // 0 = Play, 1 = Exit
	int selectedCardIndex;
	bool camSnapped;// toggle with key '9'
	bool playerFirst;
	bool newGame;
	bool gameOver;
	
	// to provide textual feedback
	TextMaker txt;

	// Other application parameters
	float Ar{};	// Aspect ratio

	glm::mat4 ViewPrj{};
	glm::mat4 World{};
	glm::vec3 Pos = glm::vec3(0,0,5);
	glm::vec3 cameraPos{};
	float Yaw = glm::radians(0.0f);
	float Pitch = glm::radians(0.0f);
	float Roll = glm::radians(0.0f);

	// Here you set the main application parameters
	void setWindowParameters() {
		// window size, titile and initial background
		windowWidth = 800;
		windowHeight = 600;
		windowTitle = "CG Project - Briscola";
    	windowResizable = GLFW_TRUE;

		// Initial aspect ratio
		Ar = 4.0f / 3.0f;
	}

	// What to do when the window changes size
	void onWindowResize(int w, int h) {
		std::cout << "Window resized to: " << w << " x " << h << "\n";
		Ar = (float)w / (float)h;
		// Update Render Pass
		RP.width = w;
		RP.height = h;

		// updates the textual output
		txt.resizeScreen(w, h);
	}

	// Here you load and setup all your Vulkan Models and Texutures.
	// Here you also create your Descriptor set layouts and load the shaders for the pipelines
	void localInit() {
		// Descriptor Layouts [what will be passed to the shaders]
		DSLglobal.init(this, {
					// this array contains the binding:
					// first  element : the binding number
					// second element : the type of element (buffer or texture)
					// third  element : the pipeline stage where it will be used
					{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, sizeof(GlobalUniformBufferObject), 1}
				  });

		DSLlocalChar.init(this, {
					// this array contains the binding:
					// first  element : the binding number
					// second element : the type of element (buffer or texture)
					// third  element : the pipeline stage where it will be used
					{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObjectChar), 1},
					{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
				  });

		DSLlocalSimp.init(this, {
					// this array contains the binding:
					// first  element : the binding number
					// second element : the type of element (buffer or texture)
					// third  element : the pipeline stage where it will be used
					{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObjectSimp), 1},
					{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1},
					{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1}
				  });

		DSLskyBox.init(this, {
			{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(skyBoxUniformBufferObject), 1},
			{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
		  });

		DSLlocalPBR.init(this, {
					// this array contains the binding:
					// first  element : the binding number
					// second element : the type of element (buffer or texture)
					// third  element : the pipeline stage where it will be used
					{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObjectSimp), 1},
					{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1},
					{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1},
					{3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2, 1},
                    {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3, 1}
				  });
		DSLlocalCard.init(this, {
			{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObjectCard), 1}, // binding 0, UBO
			{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}, // binding 1, uAtlas
			{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1}  // binding 2, uBack
		});




		VDchar.init(this, {
				  {0, sizeof(VertexChar), VK_VERTEX_INPUT_RATE_VERTEX}
				}, {
				  {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexChar, pos),
				         sizeof(glm::vec3), POSITION},
				  {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexChar, norm),
				         sizeof(glm::vec3), NORMAL},
				  {0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexChar, UV),
				         sizeof(glm::vec2), UV},
					{0, 3, VK_FORMAT_R32G32B32A32_UINT, offsetof(VertexChar, jointIndices),
				         sizeof(glm::uvec4), JOINTINDEX},
					{0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexChar, weights),
				         sizeof(glm::vec4), JOINTWEIGHT}
				});

		VDsimp.init(this, {
				  {0, sizeof(VertexSimp), VK_VERTEX_INPUT_RATE_VERTEX}
				}, {
				  {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexSimp, pos), sizeof(glm::vec3), POSITION},
				  {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexSimp, norm), sizeof(glm::vec3), NORMAL},
				  {0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexSimp, UV), sizeof(glm::vec2), UV}
				});

		VDskyBox.init(this, {
		  {0, sizeof(skyBoxVertex), VK_VERTEX_INPUT_RATE_VERTEX}
		}, {
		  {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(skyBoxVertex, pos),
				 sizeof(glm::vec3), POSITION}
		});

		VDtan.init(this, {
				  {0, sizeof(VertexTan), VK_VERTEX_INPUT_RATE_VERTEX}
				}, {
				  {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexTan, pos),
				         sizeof(glm::vec3), POSITION},
				  {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexTan, norm),
				         sizeof(glm::vec3), NORMAL},
				  {0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexTan, UV),
				         sizeof(glm::vec2), UV},
				  {0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexTan, tan),
				         sizeof(glm::vec4), TANGENT}
				});


		VDRs.resize(4);
		VDRs[0].init("VDchar",   &VDchar);
		VDRs[1].init("VDsimp",   &VDsimp);
		VDRs[2].init("VDskybox", &VDskyBox);
		VDRs[3].init("VDtan",    &VDtan);

		// initializes the render passes
		RP.init(this);
		// sets the blue sky
		RP.properties[0].clearValue = {0.0f,0.9f,1.0f,1.0f};


		// Pipelines [Shader couples]
		// The last array, is a vector of pointer to the layouts of the sets that will
		// be used in this pipeline. The first element will be set 0, and so on..



		Pchar.init(this, &VDchar, "shaders/PosNormUvTanWeights.vert.spv", "shaders/CookTorranceForCharacter.frag.spv", {&DSLglobal, &DSLlocalChar});

		PsimpObj.init(this, &VDsimp, "shaders/SimplePosNormUV.vert.spv", "shaders/CookTorrance.frag.spv", {&DSLglobal, &DSLlocalSimp});

		PskyBox.init(this, &VDskyBox, "shaders/SkyBoxShader.vert.spv", "shaders/SkyBoxShader.frag.spv", {&DSLskyBox});
		PskyBox.setCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);
		PskyBox.setCullMode(VK_CULL_MODE_BACK_BIT);
		PskyBox.setPolygonMode(VK_POLYGON_MODE_FILL);

		P_PBR.init(this, &VDtan, "shaders/SimplePosNormUvTan.vert.spv", "shaders/PBR.frag.spv", {&DSLglobal, &DSLlocalPBR});
		Pcard.init(this, &VDsimp, "shaders/card.vert.spv", "shaders/card.frag.spv", {&DSLglobal, &DSLlocalCard});
		Pcard.setCullMode(VK_CULL_MODE_NONE);
		//Pcard.setFrontFace(VK_FRONT_FACE_CLOCKWISE);

		PRs.resize(5);
		PRs[0].init("CookTorranceChar", {
							 {&Pchar, {//Pipeline and DSL for the first pass
								 /*DSLglobal*/{},
								 /*DSLlocalChar*/{
										/*t0*/{true,  0, {}}// index 0 of the "texture" field in the json file
									 }
									}}
							  }, /*TotalNtextures*/1, &VDchar);
		PRs[1].init("CookTorranceNoiseSimp", {
							 {&PsimpObj, {//Pipeline and DSL for the first pass
								 /*DSLglobal*/{},
								 /*DSLlocalSimp*/{
										/*t0*/{true,  0, {}},// index 0 of the "texture" field in the json file
										/*t1*/{true,  1, {}} // index 1 of the "texture" field in the json file
									 }
									}}
							  }, /*TotalNtextures*/2, &VDsimp);
		PRs[2].init("SkyBox", {
							 {&PskyBox, {//Pipeline and DSL for the first pass
								 /*DSLskyBox*/{
										/*t0*/{true,  0, {}}// index 0 of the "texture" field in the json file
									 }
									}}
							  }, /*TotalNtextures*/1, &VDskyBox);
		PRs[3].init("PBR", {
							 {&P_PBR, {//Pipeline and DSL for the first pass
								 /*DSLglobal*/{},
								 /*DSLlocalPBR*/{
										/*t0*/{true,  0, {}},// index 0 of the "texture" field in the json file
										/*t1*/{true,  1, {}},// index 1 of the "texture" field in the json file
										/*t2*/{true,  2, {}},// index 2 of the "texture" field in the json file
										/*t3*/{true,  3, {}}// index 3 of the "texture" field in the json file
									 }
									}}
							  }, /*TotalNtextures*/4, &VDtan);

		PRs[4].init("CardTechnique", {
			{&Pcard, {
				/*DSLglobal*/{},
				/*DSLlocalCard*/{
					/*t0*/{true,  0, {}}, // binding 1 → uAtlas
					/*t1*/{true,  1, {}}  // binding 2 → uBack
				}
			}}
		}, /*TotalNtextures*/2, &VDsimp);
		// Models, textures and Descriptors (values assigned to the uniforms)

		// sets the size of the Descriptor Set Pool
		DPSZs.uniformBlocksInPool = 3;
		DPSZs.texturesInPool = 6;
		DPSZs.setsInPool = 4;

		std::cout << "\nLoading the scene\n\n";
		if(SC.init(this, /*Npasses*/1, VDRs, PRs, "assets/models/scene.json") != 0) {
			std::cout << "ERROR LOADING THE SCENE\n";
			exit(0);
		}
		// initializes animations

		// initializes the textual output
		txt.init(this, windowWidth, windowHeight);

		// submits the main command buffer
		submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

		// Prepares for showing the FPS count
		txt.print(1.0f, 1.0f, "FPS:",1,"CO",false,false,true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});

		gameState = GameState::MENU;
		camSnapped = true;
		menuIndex = 0;
		selectedCardIndex = -1;
		ca = std::make_unique<CardAnimator>(
			[this](int idx, const glm::mat4& M){ SC.TI[4].I[idx].Wm = M; },
			[this](int idx){ return SC.TI[4].I[idx].Wm; }
		);
	}

	// Here you create your pipelines and Descriptor Sets!
	void pipelinesAndDescriptorSetsInit() {
		// creates the render pass
		RP.create();

		// This creates a new pipeline (with the current surface), using its shaders for the provided render pass
		Pchar.create(&RP);
		PsimpObj.create(&RP);
		PskyBox.create(&RP);
		P_PBR.create(&RP);
		Pcard.create(&RP);

		SC.pipelinesAndDescriptorSetsInit();
		txt.pipelinesAndDescriptorSetsInit();
	}

	// Here you destroy your pipelines and Descriptor Sets!
	void pipelinesAndDescriptorSetsCleanup() {
		Pchar.cleanup();
		PsimpObj.cleanup();
		PskyBox.cleanup();
		P_PBR.cleanup();
		Pcard.cleanup();
		RP.cleanup();

		SC.pipelinesAndDescriptorSetsCleanup();
		txt.pipelinesAndDescriptorSetsCleanup();
	}

	// Here you destroy all the Models, Texture and Desc. Set Layouts you created!
	// You also have to destroy the pipelines
	void localCleanup() {
		DSLlocalChar.cleanup();
		DSLlocalSimp.cleanup();
		DSLlocalPBR.cleanup();
		DSLskyBox.cleanup();
		DSLglobal.cleanup();
		DSLlocalCard.cleanup();

		Pchar.destroy();
		PsimpObj.destroy();
		PskyBox.destroy();
		P_PBR.destroy();
		Pcard.destroy();

		RP.destroy();

		SC.localCleanup();
		txt.localCleanup();

		//gc.stop();
	}

	// Here it is the creation of the command buffer:
	// You send to the GPU all the objects you want to draw,
	// with their buffers and textures
	static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *Params) {
		// Simple trick to avoid having always 'T->'
		// in che code that populates the command buffer!
		BRISCOLA *T = (BRISCOLA *)Params;
		T->populateCommandBuffer(commandBuffer, currentImage);
	}
	// This is the real place where the Command Buffer is written
	void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {

		// begin standard pass
		RP.begin(commandBuffer, currentImage);

		SC.populateCommandBuffer(commandBuffer, 0, currentImage);

		RP.end(commandBuffer);
	}


	//Game Animations
	void initCardPosition() {
		// Put player cards back to their base (rest) slots.
		// Slots (left, middle, right) match how you already place them.
		const float offset = 0.06325f;
		static const glm::vec3 baseSlots[3] = {
			glm::vec3(-offset, 0.75f,  0.5f), // left
			glm::vec3( 0.0f,   0.75f,  0.5f), // middle
			glm::vec3( offset, 0.75f,  0.5f)  // right
		};

		const int n = static_cast<int>(playerCards.size());
		for (int i = 0; i < n && i < 3; ++i) {
			int id = playerCards[i].id;
			glm::mat4 M = SC.TI[4].I[id].Wm;          // keep current rotation/scale
			M[3] = glm::vec4(baseSlots[i], 1.0f);     // replace only translation
			SC.TI[4].I[id].Wm = M;
		}

	}

	void highlightCard(int index) {
		if (index < 0 || index >= static_cast<int>(playerCards.size())) return;

		// First reset everyone to their base slots
		initCardPosition();

		// raise the selected one slightly (on Y only)
		const float offset = 0.06325f;
		static const glm::vec3 baseSlots[3] = {
			glm::vec3(-offset, 0.75f,  0.5f),
			glm::vec3( 0.0f,   0.75f,  0.5f),
			glm::vec3( offset, 0.75f,  0.5f)
		};

		const float raise = 0.035f;  // tweakable
		int id = playerCards[index].id;
		glm::mat4 M = SC.TI[4].I[id].Wm;
		glm::vec3 raised = baseSlots[index] + glm::vec3(0.0f, raise, 0.0f);
		M[3] = glm::vec4(raised, 1.0f);
		SC.TI[4].I[id].Wm = M;
	}

	void moveToCenter(bool isPlayer, int id1, const glm::mat4& cur1) {
		float p = isPlayer ? 1.0f : -1.0f;
		float y = isPlayer ? 0.56325f : 0.563f;
		glm::vec3 tableCenter(0.03f*(-p), y, 0.0f);
		if(!isPlayer){
			ca->addRotate(id1, cur1, 180.0f, glm::vec3(0,0,1), 0.0f, false);
		}
		ca->addMoveAndRotate(
			id1,
			cur1,
			tableCenter,
			0.8f,
			-90.0f*p, glm::vec3(1,0,0),
			0.8f,
			false
		);
	}

	void moveToCenterBoth(bool isPlayer, int id1, const glm::mat4& cur1, int id2, const glm::mat4& cur2){
		float p = isPlayer ? 1.0f : -1.0f;
		//glm::vec3 tableCenter(0.0f, 0.563f, 0.0f);

		ca->addMoveAndRotate(
			id1,
			cur1,
			glm::vec3(-0.03f, 0.563f, 0.0f) ,
			0.8f,
			-90.0f*p, glm::vec3(1,0,0),
			0.8f,
			false
		);
		ca->addRotate(id2, cur2, 180.0f, glm::vec3(0,0,1), 0.0f, false);
		ca->addMoveAndRotate(
			id2,
			cur2,
			glm::vec3(0.03f, 0.56325f, 0.0f),
			0.8f,
			90.0f*p, glm::vec3(1,0,0),
			0.8f,
			false
		);
	}

	void moveToPile(std::vector<Card> pile, glm::vec3 pilePos, int id1, const glm::mat4& cur1, int id2, const glm::mat4& cur2){
		float pileSize = 0.00025f;
		float pileTop = pile.size()*pileSize;
		ca->addMove(id1, cur1, pilePos + glm::vec3(0.0f, pileTop, 0.0f), 0.8f);
		ca->addMove(id2, cur2, pilePos + glm::vec3(0.0f, pileTop + pileSize, 0.0f), 0.8f);
	}

	void sortHand(bool isPlayer, int choice) {
		//std::cout << (isPlayer ? "Player" : "CPU") << " sorts hand, choice = " << choice << "\n";
		auto& hand = isPlayer ? playerCards : cpuCards;
		if (hand.size() <= 1) return;          // nothing to do safely
		//std::cout << "Sorting hand, choice = " << choice << "\n";
		if(choice == 2) {
			hand.erase(hand.begin()+2);
			return;
		}

		float p = isPlayer ? 1.0f : -1.0f;
		const float offset = 0.06325f;


		// If the middle card (index 1) was played, slide the right card to middle:
		if (choice == 1) {
			if (hand.size() == 3) {
				int id = hand.at(2).id;
				glm::mat4 cur = SC.TI[4].I[id].Wm;
				ca->addMove(id, cur, glm::vec3(0, 0.75f, 0.5f * p), 0.8f);
			}
			// shift vector contents
			hand.erase(hand.begin()+1);
			return;
		}

		int id;
		glm::mat4 cur;
		//choice 0
		if(hand.size() > 1){
			id = hand.at(1).id;
			cur = SC.TI[4].I[id].Wm;
			ca->addMove(id, cur, glm::vec3(-offset * p, 0.75f, 0.5f * p), 0.8f);
		}
		if(hand.size() > 2){
			id = hand.at(2).id;
			cur = SC.TI[4].I[id].Wm;
			ca->addMove(id, cur, glm::vec3(0, 0.75f, 0.5f * p), 0.8f);
		}
		hand.erase(hand.begin());
	}

	void drawCardToHand(bool isPlayer, int cardIndex) {
		//std::cout << (isPlayer ? "Player" : "CPU") << " draws a card\n";

		float p = -1.0f;
		if (isPlayer) p = 1.0f;
		float offset = 0.06325f;
		std::vector<Card> cards = gc.getDeck();
		if(cards.size() == 0) return;
		Card c = cards.at(cardIndex);
		if(isPlayer){
			playerCards.push_back(c);
		}else{
			cpuCards.push_back(c);
		}
		int id = c.id;
		glm::mat4 cur =  SC.TI[4].I[id].Wm;
		ca->addWait(id, cur, 1.8f);
		if(c.id != cards.back().id){
			if(isPlayer){
				ca->addMoveAndRotate(
					id,
					cur,
					glm::vec3(0.0f, 0.563f, 0.2f*p),
					0.8f,
					180.0f, glm::vec3(0,1, 0),
					0.8f,
					false
				);
			}else{
				ca->addMoveAndRotate(
					id,
					cur,
					glm::vec3(0.0f, 0.563f, 0.2f*p),
					0.8f,
					0.0f, glm::vec3(0,1, 0),
					0.8f,
					false
				);
			}
				ca->addMoveAndRotate(
					id,
					cur,
					glm::vec3(offset*p, 0.75f, 0.5f*p),
					0.8f,
					-90.0f*p, glm::vec3(1,0, 0),
					0.8f,
					false
				);
		}else{
			ca->addMoveAndRotate(
					id,
					cur,
					glm::vec3(0.0f, 0.563f, 0.2f*p),
					0.8f,
					90.0f*p, glm::vec3(0,1, 0),
					0.8f,
					false
			);
			ca->addMoveAndRotate(
				id,
				cur,
				glm::vec3(offset*p, 0.75f, 0.5f*p),
				0.8f,
				90.0f*p, glm::vec3(1,0, 0),
				0.8f,
				false
			);
		}


	}

	void drawNewRound(){
		float animDur = 0.8f; //usually 0.8f
		float animWait = 0.5f; // usually 0.5f
		glm::vec3 basePos(-0.177f, 0.563f, 0.0f);
		float g = gameOver? 0.0f : 1.0f;
		std::vector<Card> cards = gc.getDeck();

		//Deck
		float i = 0.0f;
		for(int j = cards.size() - 1; j >= 0; --j) {
			int id = cards[j].id;
			float yOffset = 0.00025f * i;
			glm::vec3 pos = basePos + glm::vec3(0.0f, yOffset, 0.0f);
			glm::mat4 cur = SC.TI[4].I[id].Wm;
			ca->addMoveAndRotate(
				id,
				cur,
				pos,
				animDur*g,
				180.0f, glm::vec3(0,0,1),
				animDur*g,
				false
			);
			i=i+1.0f;
		}

		//Briscola
		int id = cards.back().id;
		glm::mat4 cur = SC.TI[4].I[id].Wm;
		ca->addMove  (id, cur, glm::vec3(0.0f, 0.563f, 0.0f), 0.8f);
		ca->addMoveAndRotate(
			id,
			cur,
			glm::vec3(0.0f, 0.593f, 0.0f),
			animDur/2,
			180.0f, glm::vec3(0,0,1),
			animDur ,
			false
		);
		ca->addWait(id, cur, animWait*2);
		ca->addMoveAndRotate(
			id,
			cur,
			glm::vec3(-0.16f, 0.563f, 0.0f),
			animDur,
			-90.0f, glm::vec3(0,1, 0),
			animDur ,
			false
		);
		ca->addGlobalWait(animWait*7);

		//Hands
		playerCards.clear();
		cpuCards.clear();
		i=0.0f;
		float offset = 0.06325f;
		for (int j=0; j<6; j=j+2) {
			id = cards.at(j).id;
			cur = SC.TI[4].I[id].Wm;
			ca->addMoveAndRotate(
				id,
				cur,
				glm::vec3(0.0f, 0.563f, 0.2f),
				animDur ,
				180.0f, glm::vec3(0,1, 0),
				animDur ,
				false
			);
			ca->addMoveAndRotate(
				id,
				cur,
				glm::vec3(-offset+i, 0.75f, 0.5f),
				animDur ,
				-90.0f, glm::vec3(1,0, 0),
				animDur ,
				false
			);
			ca->addGlobalWait(animWait);
			id = cards.at(j+1).id;
			cur = SC.TI[4].I[id].Wm;
			ca->addMove  (id, cur, glm::vec3(0.0f, 0.563f, -0.2f), 0.8f);
			ca->addMoveAndRotate(
				id,
				cur,
				glm::vec3(offset-i, 0.75f, -0.5f),
				animDur ,
				90.0f, glm::vec3(1,0, 0),
				animDur ,
				false
			);
			ca->addGlobalWait(animWait);
			i+=offset;
			//std::cout << "ello " << j << " x " << i << "\n";
			playerCards.push_back(cards.at(j));
			cpuCards.push_back(cards.at(j + 1));
		}
	}

	void play(int playerChoice){
		glm::vec3 playerPilePos(0.18f, 0.563f, 0.18f);
		glm::vec3 cpuPilePos(-0.18f, 0.563f, -0.18f);
		Card playerCard = playerCards.at(playerChoice);
		Card cpuCard = cpuCards.at(cpuChoice);
		int pId = playerCard.id;
		int cId = cpuCard.id;
		cpuCardId = cId;
		glm::mat4 pCur =  SC.TI[4].I[pId].Wm;
		glm::mat4 cCur = SC.TI[4].I[cId].Wm;


		if(playerFirst){
			moveToCenterBoth(true, pId, pCur, cId, cCur);
		}else{
			ca->addWait(cId, cCur, 0.8f);
			moveToCenter(true, pId, pCur);
		}
		sortHand(true, playerChoice);
		sortHand(false, cpuChoice);

		ca->addWait(pId, pCur, 1.0f);
		ca->addWait(cId, cCur, 1.0f);

		// 1.8 sec for the previous animations to complete

		//std::cout << "I exist sadasdasdasdasdasd\n";
		//std::cout << "Player plays card index " << playerChoice << " (id=" << playerCard.id << ")\n";
		bool playerWins = gc.playTurn(playerChoice, cpuChoice);
		//std::cout << (playerWins ? "Player wins the turn\n" : "CPU wins the turn\n");
		if(playerWins) {
			if(playerFirst){
				playerPile.push_back(playerCard);
				playerPile.push_back(cpuCard);
				moveToPile(playerPile, playerPilePos, pId, pCur, cId, cCur);
			}else{
				playerPile.push_back(cpuCard);
				playerPile.push_back(playerCard);
				moveToPile(playerPile, playerPilePos, cId, cCur, pId, pCur);
			}


			playerFirst = true;
			if(gc.getDeck().size() > 0) {
				drawCardToHand(true, 0);
				drawCardToHand(false, 1);
				gc.drawCards(playerWins);
			}

		} else {
			if(playerFirst){
				cpuPile.push_back(playerCard);
				cpuPile.push_back(cpuCard);
				moveToPile(cpuPile, cpuPilePos, pId, pCur, cId, cCur);
			}else{
				cpuPile.push_back(cpuCard);
				cpuPile.push_back(playerCard);
				moveToPile(cpuPile, cpuPilePos, cId, cCur, pId, pCur);
			}

			playerFirst = false;
			if(gc.getDeck().size() > 0) {
				drawCardToHand(false, 0);
				drawCardToHand(true, 1);
				gc.drawCards(playerWins);
			}
		}

		if(gc.getDeck().size() == 0 && gc.getPlayerHandSize() == 0) {
			gameOver = true;
			gc.displayFinalResult();
			return;
		}

		// takes 1.8 + 1.6 = 3.4 sec to complete prev animations


		cpuChoice = std::rand() % gc.getCpuHandSize();
		if (!playerFirst) {
			cpuCard = cpuCards.at(cpuChoice);
			cId = cpuCard.id;
			cpuCardId = cId;
			cCur = SC.TI[4].I[cId].Wm;
			// 1.8 + 1.6 = 3.4 to finish all prev anims
			// 3.4 - 1.6 = 1.8 for when the card is the one from draw
			// 3.8 - 0.8 = 2.6 for when the card is sorted in hand
			if(cpuChoice == 2){
				ca->addWait(cId, cCur, 1.8f);
			}else{
				ca->addWait(cId, cCur, 2.6f);
			}
			moveToCenter(false, cId, cCur);
		}
	}

	// Here is where you update the uniforms.
	// Very likely this will be where you will be writing the logic of your application.
	void updateUniformBuffer(uint32_t currentImage) {
		static bool debounce = false;
		static int curDebounce = 0;

		// ==========================
		// INIT MENU
		// ==========================
		if (gameState == GameState::MENU) {
			if (glfwGetKey(window, GLFW_KEY_UP) && !debounce) {
				debounce = true; curDebounce = GLFW_KEY_UP;
				menuIndex = (menuIndex + 1) % 2; // toggle between 0 and 1
			} else if ((curDebounce == GLFW_KEY_UP) && debounce) {
				debounce = false; curDebounce = 0;
			}

			if (glfwGetKey(window, GLFW_KEY_DOWN) && !debounce) {
				debounce = true; curDebounce = GLFW_KEY_DOWN;
				menuIndex = (menuIndex + 1) % 2;
			} else if ((curDebounce == GLFW_KEY_DOWN) && debounce) {
				debounce = false; curDebounce = 0;
			}

			if (glfwGetKey(window, GLFW_KEY_SPACE) && !debounce) {
				debounce = true; curDebounce = GLFW_KEY_SPACE;

				if (menuIndex == 0) {
					// Play
					gameState = GameState::PLAYING; //PLAYING SELECTED
					gc.run();
					newGame = true;
					gameOver = true;
					playerFirst = true;
					cpuChoice = 0;
					cpuCardId = 0;
				} else {
					// Exit
					glfwSetWindowShouldClose(window, GL_TRUE);
				}
			} else if ((curDebounce == GLFW_KEY_SPACE) && debounce) {
				debounce = false; curDebounce = 0;
			}

			// Render the menu text only if in the MENU state
			txt.print(0.0f, 0.2f, "PLAY", 10, "CO", false, false, true,
					  TAL_CENTER, TRH_CENTER, TRV_MIDDLE,
					  (menuIndex == 0) ? glm::vec4(1, 0.5, 0, 1) : glm::vec4(1, 1, 1, 1),
					  {0, 0, 0, 1});

			txt.print(0.0f, -0.2f, "EXIT", 11, "CO", false, false, true,
					  TAL_CENTER, TRH_CENTER, TRV_MIDDLE,
					  (menuIndex == 1) ? glm::vec4(1, 0.5, 0, 1) : glm::vec4(1, 1, 1, 1),
					  {0, 0, 0, 1});

			txt.updateCommandBuffer();  // Render the menu
			return;  // Skip the rest of the game logic while in MENU state
		}

		// ==========================
		// GAME LOGIC
		// ==========================
		if (gameState == GameState::PLAYING) {

			//Reset menu text
			txt.print(0.0f, 0.2f, "", 10, "CO", false, false, true,
					  TAL_CENTER, TRH_CENTER, TRV_MIDDLE,
					  (menuIndex == 0) ? glm::vec4(1, 0.5, 0, 1) : glm::vec4(1, 1, 1, 1),
					  {0, 0, 0, 1});

			txt.print(0.0f, -0.2f, "", 11, "CO", false, false, true,
					  TAL_CENTER, TRH_CENTER, TRV_MIDDLE,
					  (menuIndex == 1) ? glm::vec4(1, 0.5, 0, 1) : glm::vec4(1, 1, 1, 1),
					  {0, 0, 0, 1});

			// handle the ESC key to exit the app
			if(glfwGetKey(window, GLFW_KEY_ESCAPE)) {
				glfwSetWindowShouldClose(window, GL_TRUE);
			}

			// ==========================
			// CARD SELECTION WITH ARROWS
			// ==========================
			if (glfwGetKey(window, GLFW_KEY_LEFT)) {
				if (!debounce) {
					debounce = true;
					curDebounce = GLFW_KEY_LEFT;

					selectedCardIndex--;
					if (selectedCardIndex < 0) {
						selectedCardIndex = static_cast<int>(playerCards.size()) - 1; // wrap to last
					}

					initCardPosition();
					highlightCard(selectedCardIndex);
				}
			} else if ((curDebounce == GLFW_KEY_LEFT) && debounce) {
				debounce = false;
				curDebounce = 0;
			}

			if (glfwGetKey(window, GLFW_KEY_RIGHT)) {
				if (!debounce) {
					debounce = true;
					curDebounce = GLFW_KEY_RIGHT;

					selectedCardIndex++;
					if (selectedCardIndex >= static_cast<int>(playerCards.size())) {
						selectedCardIndex = 0; // wrap to first
					}

					initCardPosition();
					highlightCard(selectedCardIndex);
				}
			} else if ((curDebounce == GLFW_KEY_RIGHT) && debounce) {
				debounce = false;
				curDebounce = 0;
			}

			//spacebar to confirm
			if (glfwGetKey(window, GLFW_KEY_SPACE)) {
				if (!debounce) {
					debounce = true;
					curDebounce = GLFW_KEY_SPACE;

					if (!gameOver && !playerCards.empty()) {
						// Confirm play of the selected card
						int idx = selectedCardIndex;
						play(idx);
						selectedCardIndex = -1; //reset the index
					}
				}
			} else if ((curDebounce == GLFW_KEY_SPACE) && debounce) {
				debounce = false;
				curDebounce = 0;
			}


			//INPUT WITH NUMBERS FOR DEBUG
			if(glfwGetKey(window, GLFW_KEY_1)) {
				if(!debounce) {
					debounce = true;
					curDebounce = GLFW_KEY_1;

					if(!gameOver){
						if(gc.IsPlayerTurn() and ca->isFinished(cpuCardId)){
							play(0);
						}else if(ca->isFinished(cpuCardId)){
							play(0);
						}
					}
				}
			} else {
				if((curDebounce == GLFW_KEY_1) && debounce) {
					debounce = false;
					curDebounce = 0;
				}
			}

			if(glfwGetKey(window, GLFW_KEY_2)) {
				if(!debounce) {
					debounce = true;
					curDebounce = GLFW_KEY_2;

					if(!gameOver and gc.getPlayerHandSize() > 1){
						if(gc.IsPlayerTurn() or ca->isFinished(cpuCardId)){
							play(1);
						}else if(ca->isFinished(cpuCardId)){
							play(1);
						}
					}
				}
			} else {
				if((curDebounce == GLFW_KEY_2) && debounce) {
					debounce = false;
					curDebounce = 0;
				}
			}

			if(glfwGetKey(window, GLFW_KEY_3)) {
				if(!debounce) {
					debounce = true;
					curDebounce = GLFW_KEY_3;

					if(!gameOver and gc.getPlayerHandSize() > 2){
						if(gc.IsPlayerTurn()or ca->isFinished(cpuCardId)){
							play(2);
						}else if(ca->isFinished(cpuCardId)){
							play(2);
						}
					}
				}
			} else {
				if((curDebounce == GLFW_KEY_3) && debounce) {
					debounce = false;
					curDebounce = 0;
				}
			}

			if(glfwGetKey(window, GLFW_KEY_R)) {
				if(!debounce) {
					debounce = true;
					curDebounce = GLFW_KEY_R;

					if (gameOver) {
						newGame = true;
						gameOver = false;
						gc.resetGame();
					}


				}
			} else {
				if((curDebounce == GLFW_KEY_R) && debounce) {
					debounce = false;
					curDebounce = 0;
				}
			}

			if (glfwGetKey(window, GLFW_KEY_9)) {
				if (!debounce) {
					debounce = true;
					curDebounce = GLFW_KEY_9;

					camSnapped = !camSnapped;   // toggle snap mode
					std::cout << (camSnapped ? "Camera SNAP ON\n" : "Camera SNAP OFF\n");
				}
			} else {
				if ((curDebounce == GLFW_KEY_9) && debounce) {
					debounce = false;
					curDebounce = 0;
				}
			}

			// moves the view
			float deltaT = CameraLogic();

			if(newGame) {
				drawNewRound();
				newGame = false;
				gameOver = false;
				gc.dealInitialCards();
				cpuChoice = cpuCards.empty() ? -1 : std::rand() % static_cast<int>(cpuCards.size());
			}

			// Progress card animations by delta time
			if (ca) ca->tick(deltaT);

			// defines the global parameters for the uniform
			const glm::mat4 lightView = glm::rotate(glm::mat4(1), glm::radians(-30.0f), glm::vec3(0.0f,1.0f,0.0f)) * glm::rotate(glm::mat4(1), glm::radians(-45.0f), glm::vec3(1.0f,0.0f,0.0f));
			const glm::vec3 lightDir = glm::vec3(lightView * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
			GlobalUniformBufferObject gubo{};
			gubo.lightDir = lightDir;
			gubo.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
			gubo.eyePos = cameraPos;

			// defines the local parameters for the uniforms
			int instanceId;

			UniformBufferObjectSimp ubos{};
			// normal objects
			for(instanceId = 0; instanceId < SC.TI[1].InstanceCount; instanceId++) {
				ubos.mMat   = SC.TI[1].I[instanceId].Wm;
				ubos.mvpMat = ViewPrj * ubos.mMat;
				ubos.nMat   = glm::inverse(glm::transpose(ubos.mMat));

				SC.TI[1].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
				SC.TI[1].I[instanceId].DS[0][1]->map(currentImage, &ubos, 0);  // Set 1
			}

			// skybox pipeline
			skyBoxUniformBufferObject sbubo{};
			sbubo.mvpMat = ViewPrj * glm::translate(glm::mat4(1), cameraPos) * glm::scale(glm::mat4(1), glm::vec3(100.0f));
			SC.TI[2].I[0].DS[0][0]->map(currentImage, &sbubo, 0);

			// PBR objects
			for(instanceId = 0; instanceId < SC.TI[3].InstanceCount; instanceId++) {
				ubos.mMat   = SC.TI[3].I[instanceId].Wm;
				ubos.mvpMat = ViewPrj * ubos.mMat;
				ubos.nMat   = glm::inverse(glm::transpose(ubos.mMat));

				SC.TI[3].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
				SC.TI[3].I[instanceId].DS[0][1]->map(currentImage, &ubos, 0);  // Set 1
			}

			// CARD objects
			UniformBufferObjectCard ubos2{};
			for(instanceId = 0; instanceId < SC.TI[4].InstanceCount; instanceId++) {

				ubos2.mMat   = SC.TI[4].I[instanceId].Wm;
				ubos2.mvpMat = ViewPrj * ubos2.mMat;
				ubos2.nMat   = glm::inverse(glm::transpose(ubos2.mMat));
				ubos2.cardIndex = instanceId;
				if (selectedCardIndex >= 0 && selectedCardIndex < (int)playerCards.size()) {
					ubos2.highlightCardIndex = playerCards[selectedCardIndex].id; // actual scene ID
				} else {
					ubos2.highlightCardIndex = -1; // no card highlighted
				}

				SC.TI[4].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
				SC.TI[4].I[instanceId].DS[0][1]->map(currentImage, &ubos2, 0);  // Set 1
			}

			// === Text update section ===
			// updates the FPS
			static float elapsedT = 0.0f;
			static int countedFrames = 0;

			countedFrames++;
			elapsedT += deltaT;
			if(elapsedT > 1.0f) {
				float Fps = (float)countedFrames / elapsedT;

				std::ostringstream oss;
				oss << "FPS: " << Fps << "\n";

				txt.print(1.0f, 1.0f, oss.str(), 1, "CO", false, false, true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});

				elapsedT = 0.0f;
				countedFrames = 0;
			}
			// Player score
			txt.print(-0.9f, 0.80f, "Player: " + std::to_string(gc.getPlayerPoints()), 2, "CO", false, false, true,
					  TAL_LEFT, TRH_LEFT, TRV_TOP,
					  {1,1,1,1}, {0,0,0,1});

			// CPU score
			txt.print(0.9f, -0.80f, "CPU: " + std::to_string(gc.getCpuPoints()), 3, "CO", false, false, true,
					  TAL_RIGHT, TRH_RIGHT, TRV_TOP,
					  {1,1,1,1}, {0,0,0,1});


			// === Turn text handling ===
			static bool lastTurn = !playerFirst;   // initialize opposite so it triggers once
			static double turnMsgTimer = 0.0;
			static std::string turnMsg = "";

			bool currentTurn = playerFirst;

			// Detect turn change
			if (currentTurn != lastTurn) {
				lastTurn = currentTurn;
				turnMsgTimer = glfwGetTime();
				turnMsg = currentTurn ? "Your turn" : "CPU's turn";
			}

			// Show message only for 2 seconds
			double elapsed = glfwGetTime() - turnMsgTimer;
			if (elapsed < 2.0) {
				txt.print(0.5f, 0.10f, turnMsg,
						  4, "CO", false, false, true,
						  TAL_CENTER, TRH_CENTER, TRV_BOTTOM,
						  {1,1,1,1}, {0,0,0,1});
			} else {
				// Explicitly clear the slot so it disappears
				txt.print(0.5f, 0.10f, "",
						  4, "CO", false, false, true,
						  TAL_CENTER, TRH_CENTER, TRV_BOTTOM,
						  {0,0,0,0}, {0,0,0,0});  // fully transparent
			}

			// --- GAME OVER MESSAGE ---
			if (gameOver) {
				std::string msg = "DRAW";
				if (gc.getPlayerPoints()>60) msg = "YOU WON";
				else if (gc.getCpuPoints()>60) msg = "CPU WON";

				txt.print(0.0f, 0.0f, "GAME OVER - " + msg,
						  5, "CO", false, false, false,
						  TAL_CENTER, TRH_CENTER, TRV_MIDDLE,
						  {1,1,1,1}, {0,0,0,1});
			}

			txt.updateCommandBuffer();
		}
	}

	float CameraLogic() {
		// Parameters
		// Camera FOV-y, Near Plane and Far Plane
		const float FOVy = glm::radians(45.0f);
		const float nearPlane = 0.1f;
		const float farPlane = 100.f;
		// Player starting point
		const glm::vec3 StartingPosition = glm::vec3(0.0f, 1.0f, 0.75f);
		// Camera target height and distance
		static float camHeight = 0.5625f;
		static float camDist = 5;
		// Camera Pitch limits
		const float minPitch = glm::radians(-8.75f);
		const float maxPitch = glm::radians(60.0f);
		// Rotation and motion speed
		const float ROT_SPEED = glm::radians(120.0f);
		const float MOVE_SPEED_BASE = 2.0f;
		const float MOVE_SPEED_RUN  = 5.0f;
		const float ZOOM_SPEED = MOVE_SPEED_BASE * 1.5f;
		const float MAX_CAM_DIST =  7.5;
		const float MIN_CAM_DIST =  1.5;

		// Integration with the timers and the controllers
		float deltaT;
		glm::vec3 m = glm::vec3(0.0f), r = glm::vec3(0.0f);
		bool fire = false;
		getSixAxis(deltaT, m, r, fire);
		float MOVE_SPEED = fire ? MOVE_SPEED_RUN : MOVE_SPEED_BASE;

		// Camera logic implementation
		if (camSnapped) {
			//Projection
			glm::mat4 Prj = glm::perspective(FOVy, Ar, nearPlane, farPlane);
			Prj[1][1] *= -1;

			// Place camera at some distance above table origin, looking down
			glm::vec3 eye    = glm::vec3(0.0f, 1.0f, 0.75f); // adjust to taste
			glm::vec3 target = glm::vec3(0.0f, 0.5625f, 0.0f); // table at origin
			glm::vec3 up     = glm::vec3(0.0f, 1.0f, 0.0f);

			glm::mat4 View = glm::lookAt(eye, target, up);
			ViewPrj = Prj * View;

			cameraPos = eye;
			World = glm::mat4(1.0f);
			return deltaT;
		}

		// Current Player Position - statc variable make sure its value remain unchanged in subsequent calls to the procedure
		static glm::vec3 Pos = StartingPosition;
		static glm::vec3 oldPos;
		static int currRunState = 1;

/*		camDist = camDist - m.y * ZOOM_SPEED * deltaT;
		camDist = camDist < MIN_CAM_DIST ? MIN_CAM_DIST :
				 (camDist > MAX_CAM_DIST ? MAX_CAM_DIST : camDist);*/
		camDist = (MIN_CAM_DIST + MIN_CAM_DIST) / 2.0f;

		// To be done in the assignment
		ViewPrj = glm::mat4(1);
		World = glm::mat4(1);

		oldPos = Pos;

		static float Yaw = glm::radians(0.0f);
		static float Pitch = glm::radians(0.0f);
		static float relDir = glm::radians(0.0f);
		static float dampedRelDir = glm::radians(0.0f);
		static glm::vec3 dampedCamPos = StartingPosition;

		// World
		// Position
		glm::vec3 ux = glm::rotate(glm::mat4(1.0f), Yaw, glm::vec3(0,1,0)) * glm::vec4(1,0,0,1);
		glm::vec3 uz = glm::rotate(glm::mat4(1.0f), Yaw, glm::vec3(0,1,0)) * glm::vec4(0,0,-1,1);
		Pos = Pos + MOVE_SPEED * m.x * ux * deltaT;
		Pos = Pos - MOVE_SPEED * m.z * uz * deltaT;

		camHeight += MOVE_SPEED * m.y * deltaT;
		// Rotation
		Yaw = Yaw - ROT_SPEED * deltaT * r.y;
		Pitch = Pitch - ROT_SPEED * deltaT * r.x;
		Pitch  =  Pitch < minPitch ? minPitch :
				   (Pitch > maxPitch ? maxPitch : Pitch);

		float ef = exp(-10.0 * deltaT);
		// Rotational independence from view with damping
		if(glm::length(glm::vec3(m.x, 0.0f, m.z)) > 0.001f) {
			relDir = Yaw + atan2(m.x, m.z);
			dampedRelDir = dampedRelDir > relDir + 3.1416f ? dampedRelDir - 6.28f :
						   dampedRelDir < relDir - 3.1416f ? dampedRelDir + 6.28f : dampedRelDir;
		}
		dampedRelDir = ef * dampedRelDir + (1.0f - ef) * relDir;

		// Final world matrix computaiton
		World = glm::translate(glm::mat4(1), Pos) * glm::rotate(glm::mat4(1.0f), dampedRelDir, glm::vec3(0,1,0));

		// Projection
		glm::mat4 Prj = glm::perspective(FOVy, Ar, nearPlane, farPlane);
		Prj[1][1] *= -1;
		// View
		// Target
		glm::vec3 target = Pos + glm::vec3(0.0f, camHeight, 0.0f);

		// Camera position, depending on Yaw parameter, but not character direction
		glm::mat4 camWorld = glm::translate(glm::mat4(1), Pos) * glm::rotate(glm::mat4(1.0f), Yaw, glm::vec3(0,1,0));
		cameraPos = camWorld * glm::vec4(0.0f, camHeight + camDist * sin(Pitch), camDist * cos(Pitch), 1.0);
		// Damping of camera
		dampedCamPos = ef * dampedCamPos + (1.0f - ef) * cameraPos;

		glm::mat4 View = glm::lookAt(dampedCamPos, target, glm::vec3(0,1,0));

		ViewPrj = Prj * View;

		float vel = length(Pos - oldPos) / deltaT;

		if(vel < 0.2) {
			if(currRunState != 1) {
				currRunState = 1;
			}
		} else if(vel < 3.5) {
			if(currRunState != 2) {
				currRunState = 2;
			}
		} else {
			if(currRunState != 3) {
				currRunState = 3;
			}
		}

		return deltaT;
	}

};


// This is the main: probably you do not need to touch this!
int main() {
    BRISCOLA app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}