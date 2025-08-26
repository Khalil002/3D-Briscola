// This has been adapted from the Vulkan tutorial
#include <sstream>

#include <json.hpp>

#include "modules/Starter.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"
#include "modules/Animations.hpp"
#include <glm/gtx/quaternion.hpp>
#include "gamecontroller.h"
//#include <boost/thread.hpp>
#include <future>
std::ostream& operator<<(std::ostream& os, const glm::mat4& mat) {
    for (int row = 0; row < 4; ++row) {
        os << "| ";
        for (int col = 0; col < 4; ++col) {
            os << mat[col][row] << " "; // GLM is column-major!
        }
        os << "|\n";
    }
    return os;
}

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
	int _pad[3];
};

struct skyBoxUniformBufferObject {
	alignas(16) glm::mat4 mvpMat;
};

struct cardInstance {
	Card card;
	Instance instance;
};

struct CardAnim {
    int   techIdx   = 4;          // technique index for cards (SC.TI[4])
    int   instIdx   = 0;          // which card instance
    bool  active    = false;

    // timing
    float duration  = 0.6f;       // seconds
    float elapsed   = 0.0f;

    // transform endpoints
    glm::vec3 p0{}, p1{};
    glm::quat q0{1,0,0,0}, q1{1,0,0,0};
    glm::vec3 s0{1,1,1}, s1{1,1,1};

    // optional: flip mid‑way (e.g., change cardIndex at 50%)
    bool  doFaceSwap = false;
    bool  swapped    = false;

	// Inside CardAnim (you already have decomposeTRS)
	void startMoveFromCurrent(int tIdx, int iIdx, const glm::mat4& curWm,
							glm::vec3 to, float seconds) {
		techIdx = tIdx; instIdx = iIdx;

		glm::vec3 Tcur, Scur; glm::quat Rcur;
		decomposeTRS(curWm, Tcur, Rcur, Scur);   // keep current flip/orientation

		p0 = Tcur;   p1 = to;                    // animate position only
		q0 = Rcur;   q1 = Rcur;                  // keep rotation
		s0 = Scur;   s1 = Scur;                  // keep scale

		duration = seconds;
		elapsed  = 0.0f;
		active   = true;

		doFaceSwap = false; swapped = false;
	}

    void startMove(int tIdx, int iIdx, glm::vec3 from, glm::vec3 to, float seconds) {
        techIdx = tIdx; instIdx = iIdx;
        // Extract current rotation/scale from current Wm if you track them separately,
        // else assume identity rotation/scale for a simple slide.
        p0 = from; p1 = to;
        q0 = glm::quat(1,0,0,0); q1 = q0;
        s0 = s1 = glm::vec3(1.0f);
        duration = seconds; elapsed = 0.0f; active = true;
        doFaceSwap = swapped = false;
    }

    void startFlip(int tIdx, int iIdx, glm::vec3 pivot, float seconds, glm::vec3 axis = {0,1,0}) {
        techIdx = tIdx; instIdx = iIdx;
        // Keep position, scale fixed; animate rotation 0..pi about local axis at pivot
        p0 = p1 = pivot;
        q0 = glm::quat(1,0,0,0);
        q1 = glm::angleAxis(glm::pi<float>(), glm::normalize(axis)); // 180°
        s0 = s1 = glm::vec3(1.0f);
        duration = seconds; elapsed = 0.0f; active = true;
        doFaceSwap = true; swapped = false;
    }

    // Smooth easing
    static float ease(float t){ t = glm::clamp(t,0.0f,1.0f); return t*t*(3.0f - 2.0f*t); }

	static void decomposeTRS(const glm::mat4& M, glm::vec3& T, glm::quat& R, glm::vec3& S) {
    	// Extract scale as column lengths
    	glm::vec3 c0 = glm::vec3(M[0]);
    	glm::vec3 c1 = glm::vec3(M[1]);
    	glm::vec3 c2 = glm::vec3(M[2]);
    	S = glm::vec3(glm::length(c0), glm::length(c1), glm::length(c2));
    	// Guard against zero scale
    	glm::vec3 invS = glm::vec3(S.x ? 1.f/S.x : 0.f, S.y ? 1.f/S.y : 0.f, S.z ? 1.f/S.z : 0.f);
    	// Normalize rotation columns
    	glm::mat3 rotM(
			c0 * invS.x,
			c1 * invS.y,
			c2 * invS.z
		);
    	R = glm::quat_cast(rotM);
    	T = glm::vec3(M[3]);
    }

	void startFlipFromCurrent(int tIdx, int iIdx, const glm::mat4& curWm,
						  float seconds, glm::vec3 axis = {0,1,0}, bool localAxis=true) {
    	techIdx = tIdx; instIdx = iIdx;

    	// Decompose current TRS
    	glm::vec3 Tcur, Scur;
    	decomposeTRS(curWm, Tcur, q0, Scur);

    	// Endpoints
    	p0 = p1 = Tcur;
    	s0 = s1 = Scur;

    	// Rotate 180° from current rotation
    	glm::quat dq = glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    	q1 = localAxis ? (q0 * dq)    // rotate in LOCAL space
					   : (dq * q0);   // rotate in WORLD space

    	duration = seconds;
    	elapsed  = 0.0f;
    	active   = true;

    	doFaceSwap = true;
    	swapped = false;
    }

    // Returns current world matrix
    glm::mat4 tick(float dt, glm::mat4 base = glm::mat4(1.0f)) {
        if(!active) return base;
        elapsed += dt;
        float u = ease(elapsed / duration);
        glm::vec3 p = glm::mix(p0, p1, u);
        glm::quat q = glm::slerp(q0, q1, u);
        glm::vec3 s = glm::mix(s0, s1, u);

        // swap face (e.g., change cardIndex) at halfway of a flip
        if(doFaceSwap && !swapped && u >= 0.5f){
            // Example: toggle the instance's card index (front value) here if you want
            // SC.TI[techIdx].I[instIdx].cardIndex = newIndex;
            swapped = true;
        }

        glm::mat4 T = glm::translate(glm::mat4(1.0f), p);
        glm::mat4 R = glm::mat4_cast(q);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), s);
        if(elapsed >= duration){ active = false; }
        return T * R * S;
    }
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
	
	// To support animation
	#define N_ANIMATIONS 5

	bool camSnapped = true;      // toggle with key '9'
	bool newGame;
	AnimBlender AB;
	Animations Anim[N_ANIMATIONS];
	SkeletalAnimation SKA;

	// Card Animation
	CardAnim cardAnim;

	// Briscola game controller
	GameController gc;

	//Card to instance mapping
	cardInstance cardInstances[40];

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
	
	glm::vec4 debug1 = glm::vec4(0);

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
		Pcard.init(this, &VDsimp, "shaders/card2.vert.spv", "shaders/card2.frag.spv", {&DSLglobal, &DSLlocalCard});
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
		for(int ian = 0; ian < N_ANIMATIONS; ian++) {
			Anim[ian].init(*SC.As[ian]);
		}
		AB.init({{0,32,0.0f,0}, {0,16,0.0f,1}, {0,263,0.0f,2}, {0,83,0.0f,3}, {0,16,0.0f,4}});
		//AB.init({{0,31,0.0f}});
		SKA.init(Anim, 5, "Armature|mixamo.com|Layer0", 0);
		
		// initializes the textual output
		txt.init(this, windowWidth, windowHeight);

		// submits the main command buffer
		submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

		gc.run();
		newGame = true;
		// Prepares for showing the FPS count
		txt.print(1.0f, 1.0f, "FPS:",1,"CO",false,false,true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});
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
		
		for(int ian = 0; ian < N_ANIMATIONS; ian++) {
			Anim[ian].cleanup();
		}

		//gc.stop();
	}

	void printCameraInfo(const glm::vec3 &eye, const glm::vec3 &target) {
		glm::vec3 dir = glm::normalize(target - eye);

		float yaw   = glm::degrees(atan2(dir.x, dir.z));
		float pitch = glm::degrees(asin(dir.y));

		std::cout << "Camera Eye: (" 
				<< eye.x << ", " << eye.y << ", " << eye.z << ")\n"
				<< "Yaw: " << yaw << " deg, "
				<< "Pitch: " << pitch << " deg\n";
	};
	// Here it is the creation of the command buffer:
	// You send to the GPU all the objects you want to draw,
	// with their buffers and textures
	static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *Params) {
		// Simple trick to avoid having always 'T->'
		// in che code that populates the command buffer!
//std::cout << "Populating command buffer for " << currentImage << "\n";
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

	// Here is where you update the uniforms.
	// Very likely this will be where you will be writing the logic of your application.
	void updateUniformBuffer(uint32_t currentImage) {
		static double prev = glfwGetTime();
		double now = glfwGetTime();
		float dt = float(now - prev);
		prev = now;
		static bool debounce = false;
		static int curDebounce = 0;
		
		// handle the ESC key to exit the app
		if(glfwGetKey(window, GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose(window, GL_TRUE);
		}


		if(glfwGetKey(window, GLFW_KEY_1)) {
			if(!debounce) {
				debounce = true;
				curDebounce = GLFW_KEY_1;

				//debug1.x = 1.0 - debug1.x;
				int idx = 0;
				const glm::mat4 cur = SC.TI[4].I[idx].Wm;
				cardAnim.startMoveFromCurrent(/*techIdx=*/4, /*instIdx=*/idx, cur,
											/*to=*/glm::vec3(0.6f, 0.75f, 0.3f),
											/*seconds=*/1.0f);

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

				//debug1.y = 1.0 - debug1.y;
				int idx = 0;
				const glm::mat4 cur = SC.TI[4].I[idx].Wm;
				// Flip around Z (you used {0,0,1}); choose localAxis=true to spin around card’s own axis
				cardAnim.startFlipFromCurrent(4, idx, cur, 1.0f, glm::vec3(0,0,1), /*localAxis=*/true);
			}
		} else {
			if((curDebounce == GLFW_KEY_2) && debounce) {
				debounce = false;
				curDebounce = 0;
			}
		}

		if(glfwGetKey(window, GLFW_KEY_P)) {
			if(!debounce) {
				debounce = true;
				curDebounce = GLFW_KEY_P;

				debug1.z = (float)(((int)debug1.z + 1) % 65);
std::cout << "Showing bone index: " << debug1.z << "\n";
			}
		} else {
			if((curDebounce == GLFW_KEY_P) && debounce) {
				debounce = false;
				curDebounce = 0;
			}
		}

		if(glfwGetKey(window, GLFW_KEY_O)) {
			if(!debounce) {
				debounce = true;
				curDebounce = GLFW_KEY_O;

				debug1.z = (float)(((int)debug1.z + 64) % 65);
std::cout << "Showing bone index: " << debug1.z << "\n";
			}
		} else {
			if((curDebounce == GLFW_KEY_O) && debounce) {
				debounce = false;
				curDebounce = 0;
			}
		}

		static int curAnim = 0;
		if(glfwGetKey(window, GLFW_KEY_SPACE)) {
			if(!debounce) {
				debounce = true;
				curDebounce = GLFW_KEY_SPACE;

				curAnim = (curAnim + 1) % 5;
				AB.Start(curAnim, 0.5);
std::cout << "Playing anim: " << curAnim << "\n";
			}
		} else {
			if((curDebounce == GLFW_KEY_SPACE) && debounce) {
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
		float deltaT = GameLogic();
		
		// updated the animation
		const float SpeedUpAnimFact = 0.85f;
		AB.Advance(deltaT * SpeedUpAnimFact);
		
		// defines the global parameters for the uniform
		const glm::mat4 lightView = glm::rotate(glm::mat4(1), glm::radians(-30.0f), glm::vec3(0.0f,1.0f,0.0f)) * glm::rotate(glm::mat4(1), glm::radians(-45.0f), glm::vec3(1.0f,0.0f,0.0f));
		const glm::vec3 lightDir = glm::vec3(lightView * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
	
		GlobalUniformBufferObject gubo{};

		gubo.lightDir = lightDir;
		gubo.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gubo.eyePos = cameraPos;

		// defines the local parameters for the uniforms
		UniformBufferObjectChar uboc{};	
		uboc.debug1 = debug1;

		SKA.Sample(AB);
		std::vector<glm::mat4> *TMsp = SKA.getTransformMatrices();
		
//printMat4("TF[55]", (*TMsp)[55]);
		
		glm::mat4 AdaptMat =
			glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)) * 
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f,0.0f,0.0f));
		
		int instanceId;
		// character
		for(instanceId = 0; instanceId < SC.TI[0].InstanceCount; instanceId++) {
			for(int im = 0; im < TMsp->size(); im++) {
				uboc.mMat[im]   = AdaptMat * (*TMsp)[im];
				uboc.mvpMat[im] = ViewPrj * uboc.mMat[im];
				uboc.nMat[im] = glm::inverse(glm::transpose(uboc.mMat[im]));
//std::cout << im << "\t";
//printMat4("mMat", ubo.mMat[im]);
			}

			SC.TI[0].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
			SC.TI[0].I[instanceId].DS[0][1]->map(currentImage, &uboc, 0);  // Set 1
		}

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
		if(newGame) {
			glm::vec3 basePos(-0.177f, 0.563f, 0.0f);
			std::vector<Card> cards = gc.getDeck();
			float i = 0.0f;
			for(int j = cards.size() - 1; j >= 0; --j) {
				float yOffset = 0.00025f * i;
				glm::vec3 pos = basePos + glm::vec3(0.0f, yOffset, 0.0f);
				glm::mat4 rot = glm::rotate(glm::mat4(1.0f),
											glm::radians(180.0f),
											glm::vec3(1.0f, 0.0f, 0.0f));
				SC.TI[4].I[cards[j].id].Wm = glm::translate(glm::mat4(1.0f), pos) * rot;
				i=i+1.0f;
			}
			newGame = false;
		}
		if (cardAnim.active) {
			SC.TI[4].I[cardAnim.instIdx].Wm = cardAnim.tick(dt); // update stored matrix
		}

		UniformBufferObjectCard ubos2{};
		for(instanceId = 0; instanceId < SC.TI[4].InstanceCount; instanceId++) {

			ubos2.mMat   = SC.TI[4].I[instanceId].Wm;
			ubos2.mvpMat = ViewPrj * ubos2.mMat;
			ubos2.nMat   = glm::inverse(glm::transpose(ubos2.mMat));
			ubos2.cardIndex = instanceId;

			SC.TI[4].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
			SC.TI[4].I[instanceId].DS[0][1]->map(currentImage, &ubos2, 0);  // Set 1
		}

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
		
		txt.updateCommandBuffer();
	}
	
	float GameLogic() {
		// Parameters
		// Camera FOV-y, Near Plane and Far Plane
		const float FOVy = glm::radians(45.0f);
		const float nearPlane = 0.1f;
		const float farPlane = 100.f;
		// Player starting point
		const glm::vec3 StartingPosition = glm::vec3(0.0, 0.0, 5);
		// Camera target height and distance
		static float camHeight = 1.5;
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

		if (camSnapped) {
			// disable all movement & rotation input
			m = glm::vec3(0.0f);
			r = glm::vec3(0.0f);
		}



		// Game Logic implementation
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

		// --- force snapped view when camSnapped is ON ---
		if (camSnapped) {
			// Lock to a top-down-ish 45° view towards the table
			Pitch = glm::radians(45.0f);          // look down 45°
			// keep current Yaw (so you see the table from your current azimuth),
			// or set a fixed one if you prefer: Yaw = glm::radians(0.0f);

			// lock camera height/distance: use a pleasant overhead distance
			camHeight = 0.75f;                    // eye height over target (table)
			camDist   = 3.0f;                     // distance from target

			// no smoothing while snapped: place camera instantly
			// (we’ll also set dampedCamPos later to cameraPos to remove lag)
		}

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

		if (camSnapped) {
			// kill damping so it snaps; also freeze dampedRelDir if you like
			dampedCamPos = cameraPos;
		} 

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

		//printCameraInfo(cameraPos, target);

		if (camSnapped) {
			// Projection
			glm::mat4 Prj = glm::perspective(glm::radians(45.0f), Ar, 0.1f, 100.0f);
			Prj[1][1] *= -1;

			// Place camera at some distance above table origin, looking down
			glm::vec3 eye    = glm::vec3(0.0f, 1.0f, 0.75f); // adjust to taste
			glm::vec3 target = glm::vec3(0.0f, 0.5625f, 0.0f); // table at origin
			glm::vec3 up     = glm::vec3(0.0f, 1.0f, 0.0f);

			glm::mat4 View = glm::lookAt(eye, target, up);
			ViewPrj = Prj * View;

			cameraPos = eye;
			World     = glm::mat4(1.0f); // freeze world if you want
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