///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ============
// manage the preparing and rendering of 3D scenes - textures, materials, lighting
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
// 
//  Edited by: Jerris English - SNHU CS Major
//  Date: July 28, 2025
//  Notes: Designed and constructed a 3D lamp model based on a 2D reference image.
//         Included component scaling, positioning, material color, and transparency.
// 
//  Date: August 3, 2025
//  Notes: Confirmed implementation of base ground plane for scene layout.
//         Verified it supports the lamp and defines the 3D environment floor.
// 
//  Date: August 10, 2025
//  Notes: Added texture shading to the lamp base (copper texture) and lamp globe 
//         (frosted glass texture) for enhanced realism. Implemented consistent 
//         alpha values across all glass components for cleaner transparency. 
//         Adjusted draw order and culling to reduce self-occlusion artifacts.
//         Created a true hemisphere for the bottom glass dome to match the shade's 
//         curvature and positioned it flush with the cylinder base. Increased inner 
//         glass funnel height and aligned its top edge with the dome for a seamless 
//         fit. Added blending enable in PrepareScene() for transparent object rendering.
//         Added LoadSceneTextures() to preload and bind all scene textures.
// 
//  Date: August 17, 2025
//  Notes: Added reflective shader materials to all render objects to improve 
//         light response and surface realism. Configured SetupSceneLights() 
//         to enable the shader lighting path, added a directional light for 
//         general fill, placed a point light at the bulb for a warm glow, and 
//         introduced a secondary point light as a soft overhead fill. Disabled 
//         unused point lights and spotlight to streamline the scene lighting.
// 
//  Date: August 24, 2025
//  Notes: Completed final visual elements in the scene. Constructed the cabinet 
//         structure, mirror frame with tile layout, and decorative display box 
//         with lid. Aligned and rescaled the lamp to sit centered on the box. 
//         Applied and bound new textures for zebra fur, mirror glass, and stitched 
//         felt chevron. Defined shader materials to match these textures and 
//         integrated all components using consistent transformations and lighting.
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glGenTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationZ * rotationY * rotationX * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/

/***********************************************************
 *  DefineObjectMaterials()
 *
 *  Purpose: Set up all surface materials used in the 3D scene.
 *           Each material defines how light interacts with
 *           the object — including base color (diffuse),
 *           shine response (specular), and gloss level (shininess).
 *
 *  Edited by: Jerris English - SNHU CS Major
 *  Date: August 24, 2025
 *
 *  Materials defined:
 *  - Copper (lamp base): warm reflective metal
 *  - Plastic black: low-shine utility finish
 *  - Glass: smooth surface with high gloss
 *  - Floor epoxy: dark with bright specular highlights
 *  - Wall plaster: soft matte surface with subtle depth
 *  - Zebra fur: light fabric with low reflectivity
 *  - Mirror: high-gloss polished reflection
 *  - Chevron fur: patterned box with light gloss
 *  - Box fur (lid): solid gray with soft sheen
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
	// copper
	OBJECT_MATERIAL copper;
	copper.diffuseColor = glm::vec3(0.72f, 0.43f, 0.20f);     // diffuse color
	copper.specularColor = glm::vec3(0.95f, 0.70f, 0.45f);    // specular color
	copper.shininess = 256.0f;                                 // shininess factor
	copper.tag = "copper";                                    // tag name
	m_objectMaterials.push_back(copper);                      // store material

	// plastic (black)
	OBJECT_MATERIAL plasticBlack;
	plasticBlack.diffuseColor = glm::vec3(0.06f, 0.06f, 0.06f);  // diffuse color
	plasticBlack.specularColor = glm::vec3(0.20f, 0.20f, 0.20f); // specular color
	plasticBlack.shininess = 8.0f;                               // shininess factor
	plasticBlack.tag = "plasticBlack";                           // tag name
	m_objectMaterials.push_back(plasticBlack);                   // store material

	// glass
	OBJECT_MATERIAL glass;
	glass.diffuseColor = glm::vec3(0.55f, 0.60f, 0.70f);         // diffuse color
	glass.specularColor = glm::vec3(1.5f, 1.5f, 1.5f);           // boosted specular to sharpen light pop
	glass.shininess = 160.0f;                                    // stronger shininess for crisp reflection
	glass.tag = "glass";                                         // tag name
	m_objectMaterials.push_back(glass);                          // store material

	// floorMat - epoxy, glossy reflective
	OBJECT_MATERIAL floorMat;
	floorMat.diffuseColor = glm::vec3(0.22f, 0.22f, 0.24f);      // darker base for epoxy
	floorMat.specularColor = glm::vec3(0.85f, 0.85f, 0.90f);     // bright specular for shine
	floorMat.shininess = 128.0f;                                 // strong gloss highlight
	floorMat.tag = "floorMat";                                   // tag name
	m_objectMaterials.push_back(floorMat);                       // store material

	// wallMat - plaster, soft matte
	OBJECT_MATERIAL wallMat;
	wallMat.diffuseColor = glm::vec3(0.60f, 0.55f, 0.45f);    // darker cream base color
	wallMat.specularColor = glm::vec3(0.02f, 0.02f, 0.02f);   // keep specular almost off
	wallMat.shininess = 4.0f;                                 // softer, broad response
	wallMat.tag = "wallMat";                                  // tag name
	m_objectMaterials.push_back(wallMat);                     // store material

	// zebraMat - soft specular fur/skin surface
	OBJECT_MATERIAL zebraMat;
	zebraMat.diffuseColor = glm::vec3(0.9f, 0.9f, 0.9f);         // bright base to help lighting show
	zebraMat.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);        // low subtle shine
	zebraMat.shininess = 10.0f;                                  // soft reflectivity
	zebraMat.tag = "zebraMat";                                   // tag name
	m_objectMaterials.push_back(zebraMat);                       // store material

	// mirrorMat - high-gloss reflective surface
	OBJECT_MATERIAL mirrorMat;
	mirrorMat.diffuseColor = glm::vec3(0.75f, 0.75f, 0.75f);      // Slightly lighter silver base
	mirrorMat.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);        // Full white specular highlight
	mirrorMat.shininess = 256.0f;                                 // Max gloss for mirror effect
	mirrorMat.tag = "mirrorMat";                                  // tag name
	m_objectMaterials.push_back(mirrorMat);

	// chevronMat – stitched fur with soft lighting
	OBJECT_MATERIAL chevronMat;
	chevronMat.diffuseColor = glm::vec3(0.85f, 0.85f, 0.85f);     // light base to help pattern show
	chevronMat.specularColor = glm::vec3(0.15f, 0.15f, 0.15f);    // low subtle reflectivity
	chevronMat.shininess = 12.0f;                                 // soft finish with a hint of gloss
	chevronMat.tag = "chevronMat";                                // tag name
	m_objectMaterials.push_back(chevronMat);                      // store material

	// boxFurMat – solid dark fur with light sheen
	OBJECT_MATERIAL boxFurMat;
	boxFurMat.diffuseColor = glm::vec3(0.60f, 0.60f, 0.60f);      // lightened gray fur tone
	boxFurMat.specularColor = glm::vec3(0.20f, 0.20f, 0.20f);     // very soft shine
	boxFurMat.shininess = 10.0f;                                   // low gloss, matte texture
	boxFurMat.tag = "boxFurMat";                                  // tag name
	m_objectMaterials.push_back(boxFurMat);                       // store material
}

/***********************************************************
 *  SetupSceneLights()
 *
 *  Configure scene lighting.
 *
 *  Edited by: Jerris English - 08/17/2025
 *  Edits:
 *    - Enabled shader lighting path (bUseLighting = true)
 *    - Configured a directional light for overall scene fill
 *    - Added point light [0] at the bulb for warm glow
 *    - Added point light [1] as a soft overhead fill
 *    - Disabled remaining point lights [2..4] and spotlight
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
	m_pShaderManager->setBoolValue("bUseLighting", true);                // enable lighting

	//** Directional Light — final boost for full-room glow **//
	m_pShaderManager->setVec3Value("directionalLight.direction", -0.2f, -1.0f, -0.3f);  // soft downward fill
	m_pShaderManager->setVec3Value("directionalLight.ambient", 0.28f, 0.28f, 0.28f);    // more ambient warmth
	m_pShaderManager->setVec3Value("directionalLight.diffuse", 0.38f, 0.38f, 0.38f);    // slightly richer surface fill
	m_pShaderManager->setVec3Value("directionalLight.specular", 0.50f, 0.50f, 0.50f);   // subtle reflective boost
	m_pShaderManager->setBoolValue("directionalLight.bActive", true);                  // ensure enabled

	// point light 0 (lamp bulb glow)
	m_pShaderManager->setVec3Value("pointLights[0].position", 0.0f, 10.15f, -3.5f);     // centered in dome
	m_pShaderManager->setVec3Value("pointLights[0].ambient", 0.22f, 0.20f, 0.15f);      // warm baseline glow
	m_pShaderManager->setVec3Value("pointLights[0].diffuse", 1.08f, 0.95f, 0.78f);      // slightly hotter yellow wash
	m_pShaderManager->setVec3Value("pointLights[0].specular", 1.25f, 1.10f, 0.90f);     // even brighter reflective highlight
	m_pShaderManager->setFloatValue("pointLights[0].constant", 1.0f);
	m_pShaderManager->setFloatValue("pointLights[0].linear", 0.11f);                    // slightly tighter falloff
	m_pShaderManager->setFloatValue("pointLights[0].quadratic", 0.038f);                // tighter decay for focused beam
	
	// point light 1 (fill)
	m_pShaderManager->setVec3Value("pointLights[1].position", 0.0f, 5.5f, -1.0f);      // centered fill light for foreground
	m_pShaderManager->setVec3Value("pointLights[1].ambient", 0.20f, 0.20f, 0.20f);     // soft ambient fill
	m_pShaderManager->setVec3Value("pointLights[1].diffuse", 0.40f, 0.40f, 0.40f);     // medium diffuse fill
	m_pShaderManager->setVec3Value("pointLights[1].specular", 0.20f, 0.20f, 0.20f);    // gentle specular reflection
	m_pShaderManager->setFloatValue("pointLights[1].constant", 1.0f);                 // attenuation constant
	m_pShaderManager->setFloatValue("pointLights[1].linear", 0.09f);                  // attenuation linear
	m_pShaderManager->setFloatValue("pointLights[1].quadratic", 0.032f);              // attenuation quadratic
	m_pShaderManager->setBoolValue("pointLights[1].bActive", true);                   // active

	// point light 2 (glow boost near top of dome)
	m_pShaderManager->setVec3Value("pointLights[2].position", 0.0f, 10.9f, -3.5f);      // lowered slightly to better reach glass dome
	m_pShaderManager->setVec3Value("pointLights[2].ambient", 0.12f, 0.10f, 0.08f);      // soft amber ambient to match bulb warmth
	m_pShaderManager->setVec3Value("pointLights[2].diffuse", 0.45f, 0.38f, 0.28f);      // medium-strength glow around top glass
	m_pShaderManager->setVec3Value("pointLights[2].specular", 0.55f, 0.50f, 0.40f);     // reflective tint to catch curved glass
	m_pShaderManager->setFloatValue("pointLights[2].constant", 1.0f);                   // attenuation constant
	m_pShaderManager->setFloatValue("pointLights[2].linear", 0.09f);                    // attenuation linear
	m_pShaderManager->setFloatValue("pointLights[2].quadratic", 0.032f);                // attenuation quadratic
	m_pShaderManager->setBoolValue("pointLights[2].bActive", true);                     // keep glow boost active

	// point light 3 (left wall)
	m_pShaderManager->setVec3Value("pointLights[3].position", -1.2f, 11.5f, -6.0f);    // aligned with mirror left panel
	m_pShaderManager->setVec3Value("pointLights[3].ambient", 0.05f, 0.045f, 0.035f);   // faint warm tone
	m_pShaderManager->setVec3Value("pointLights[3].diffuse", 0.15f, 0.13f, 0.11f);     // soft glow on wall
	m_pShaderManager->setVec3Value("pointLights[3].specular", 0.05f, 0.045f, 0.035f);  // gentle highlight
	m_pShaderManager->setFloatValue("pointLights[3].constant", 1.0f);                 // attenuation constant
	m_pShaderManager->setFloatValue("pointLights[3].linear", 0.09f);                  // attenuation linear
	m_pShaderManager->setFloatValue("pointLights[3].quadratic", 0.032f);              // attenuation quadratic
	m_pShaderManager->setBoolValue("pointLights[3].bActive", true);                   // active

	// point light 4 (right wall)
	m_pShaderManager->setVec3Value("pointLights[4].position", 1.2f, 11.5f, -6.0f);     // aligned with mirror right panel
	m_pShaderManager->setVec3Value("pointLights[4].ambient", 0.05f, 0.045f, 0.035f);   // faint warm tone
	m_pShaderManager->setVec3Value("pointLights[4].diffuse", 0.15f, 0.13f, 0.11f);     // soft glow on wall
	m_pShaderManager->setVec3Value("pointLights[4].specular", 0.05f, 0.045f, 0.035f);  // gentle highlight
	m_pShaderManager->setFloatValue("pointLights[4].constant", 1.0f);                 // attenuation constant
	m_pShaderManager->setFloatValue("pointLights[4].linear", 0.09f);                  // attenuation linear
	m_pShaderManager->setFloatValue("pointLights[4].quadratic", 0.032f);              // attenuation quadratic
	m_pShaderManager->setBoolValue("pointLights[4].bActive", true);                   // active


	//** Disable unused light types **//
	//*******************************//
	m_pShaderManager->setBoolValue("spotLight.bActive", false);                        // off
}

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 * 
 *  Edited by: Jerris English - SNHU CS Major
 *  Date: July 28, 2025
 *  Notes: Confirmed import of all necessary mesh types for lamp construction,
 *         including cylinder, tapered cylinder, torus, and sphere meshes.
 * 
 *  Edited on: August 10, 2025
 *  Notes: Added call to LoadSceneTextures() for loading all textures before 
 *         mesh setup, and enabled blending for rendering transparent objects 
 *         such as the glass dome.
 * 
 *  Edited on: August 17, 2025
 *  Notes: Added calls to DefineObjectMaterials() and SetupSceneLights() to 
 *         initialize surface properties and lighting for the scene.
 * 
 *  Edited on: August 17, 2025
 *  Notes: Added calls to DefineObjectMaterials() and SetupSceneLights() to 
 *         initialize surface properties and lighting for the scene.
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// Load all scene textures first
	LoadSceneTextures();

	DefineObjectMaterials();              // set up material tags + values
	SetupSceneLights();                   // configure directional + bulb lights

	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadSphereMesh();
	m_basicMeshes->LoadPrismMesh();
	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadTorusMesh();
	m_basicMeshes->LoadPyramid3Mesh();
	m_basicMeshes->LoadTaperedCylinderMesh();

	// define materials
	DefineObjectMaterials();    // setup surface materials

	// setup lights
	SetupSceneLights();         // initialize scene lighting
}

// =============================================================
//  Added by: Jerris English - SNHU CS Major
//  Date: August 10, 2025
//  Purpose: Loads and binds all textures used in the lamp scene,
//           including the copper base and frosted glass shade.
// 
//  Revisions (August 17, 2025):
//   - Added textures for the wall (plaster cream dark) and floor
//     (dark brown epoxy) to improve surface realism.
// 
//  Revisions (August 24, 2025):
//   - Included additional textures for new scene elements:
//     zebra fur for mirror tiles, mirror surface reflection,
//     chevron-patterned fur for the decorative box sides, and
//     dark box fur texture for the top lid surface.
// =============================================================
void SceneManager::LoadSceneTextures()
{
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Copper texture for lamp base
	CreateGLTexture("textures/agedrustic.jpg", "Copper");

	// Frosted glass texture for lamp shade
	CreateGLTexture("textures/frostedglass.jpg", "FrostedGlass");
	
	// Floor texture (dark brown epoxy concrete)
	CreateGLTexture("textures/DarkBrownEpoxyFloor.jpg", "Floor");

	// Wall texture (darker cream plaster)
	CreateGLTexture("textures/Wall_Plaster_Cream_Dark.jpg", "Wall");

	// Zebra fur texture for mirror frame tiles
	CreateGLTexture("textures/Zebra_Top.jpg", "ZebraFur");

	// Mirror surface texture (smooth silver reflection)
	CreateGLTexture("textures/Mirror.jpg", "Mirror");

	// Chevron pattern fur texture for box sides
	CreateGLTexture("textures/FeltChevron.jpg", "ChevronFur");

	// Solid dark grey fur texture for box top
	CreateGLTexture("textures/BoxFur.jpg", "BoxFur");

	BindGLTextures();
}


/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{	
	// reset render state for opaque pass
	glDisable(GL_BLEND);                                   // blending off
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);     // standard blend
	glDepthMask(GL_TRUE);                                   // write depth
	m_pShaderManager->setBoolValue("bUseLighting", true);   // enable lighting
	glEnable(GL_DEPTH_TEST);    // ensure depth testing is active for opaque geometry
	glDisable(GL_CULL_FACE);    // default: no culling for floor/wall; enable later as needed

	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	
	/****************************************************************
 *  3D Scene Construction - Created by Jerris English
 *  Date: July 28, 2025
 *
 *  The following shapes, positions, and materials are used to
 *  build the full 3D scene. Each section handles one part of
 *  the structure — from base to top — using basic transformations,
 *  color shaders, and mesh drawing calls.
 * 
 *  Revisions (Aug 10, 2025):
 *  - Added texture shading to the lamp base for a more realistic
 *    surface appearance.
 *  - Applied texture shading to the lamp globe (glass shade) to
 *    enhance the frosted glass effect with consistent alpha values
 *    and proper draw order for cleaner transparency.
 * 
 *   Revisions (Aug 17, 2025):
 *  - Added shader material calls to all render objects to apply
 *    proper lighting and surface response during rendering
 * 
 *	*  Revisions (Aug 24, 2025):
 *  - Constructed and textured the cabinet structure including
 *    base, doors, legs, and top mirror panel.
 *  - Designed the decorative box using chevron-patterned fur
 *    sides and a matching lid with soft fur material.
 *  - Added the full mirror panel above the lamp using zebra fur
 *    tiles with precise tile rotation and alternate textures.
 ****************************************************************/

 //** Floor plane **//
 //*****************//
	scaleXYZ = glm::vec3(8.0f, 1.0f, 6.0f);           // wide flat floor
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);        // sits at ground level
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("floorMat");                    // floor material for lighting
	SetShaderTexture("Floor");                        // epoxy floor texture
	SetTextureUVScale(3.0f, 3.0f);                    // repeat texture for detail
	m_basicMeshes->DrawPlaneMesh();

	//** Backdrop wall **//
	//*******************//
	scaleXYZ = glm::vec3(8.0f, 1.0f, 12.0f);                         // Wide and tall flat wall
	positionXYZ = glm::vec3(0.0f, 12.0f, -6.0f);                     // Push behind the rest of the scene
	SetTransformations(scaleXYZ, -90.0f, 0.0f, 0.0f, positionXYZ);  // Rotate to vertical
	SetShaderMaterial("wallMat");                                   // wall material for lighting
	SetShaderTexture("Wall");                                       // Apply cream plaster wall texture
	SetTextureUVScale(2.0f, 2.0f);                                  // Slightly tighter tiling for detail
	m_pShaderManager->setVec4Value(g_ColorValueName,                // no extra tint (preserve texture)
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawPlaneMesh();                                 // draw wall
	/****************************************************************/

//** MIRROR FRAME: TOP ROW **//
//****************************//

	scaleXYZ = glm::vec3(1.2f, 1.2f, 0.15f);  // Mirror tile size

//** Mirror Frame: Top Left Tile **//
	positionXYZ = glm::vec3(-1.8f, 16.0f, -5.90f);        // Far left
	SetTransformations(scaleXYZ, 0.0f, 0.0f, -90.0f, positionXYZ);
	SetShaderMaterial("zebraMat");                        // Gloss finish
	SetShaderTexture("ZebraFur");                         // Zebra pattern
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Mirror Frame: Top Mid-Left Tile **//
	positionXYZ = glm::vec3(-0.6f, 16.0f, -5.90f);        // Mid left
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // Rotate Z
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Mirror Frame: Top Mid-Right Tile **//
	positionXYZ = glm::vec3(0.6f, 16.0f, -5.90f);         // Mid right
	SetTransformations(scaleXYZ, 0.0f, 0.0f, -90.0f, positionXYZ);  // Rotate Z
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Mirror Frame: Top Right Tile **//
	positionXYZ = glm::vec3(1.8f, 16.0f, -5.90f);         // Far right
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // Rotate Z
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** MIRROR FRAME: LEFT COLUMN **//
//*******************************//

	//** Mirror Frame: Left Tile 1 **//
	scaleXYZ = glm::vec3(1.2f, 1.2f, 0.15f);              // Tile size
	positionXYZ = glm::vec3(-1.8f, 14.8f, -5.90f);        // First tile under top left
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 180.0f, positionXYZ);  // Base orientation
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Mirror Frame: Left Tile 2 **//
	positionXYZ = glm::vec3(-1.8f, 13.6f, -5.90f);        // Middle left tile
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 270.0f, positionXYZ);  // 90° rotation
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Mirror Frame: Left Tile 3 **//
	positionXYZ = glm::vec3(-1.8f, 12.4f, -5.90f);        // Bottom left vertical tile
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 180.0f, positionXYZ);  // 0° rotation
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** MIRROR FRAME: RIGHT COLUMN **//
//********************************//

	//** Mirror Frame: Right Tile 1 **//
	positionXYZ = glm::vec3(1.8f, 14.8f, -5.90f);         // First tile under top right
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 90.0f, positionXYZ);  // Base orientation
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Mirror Frame: Right Tile 2 **//
	positionXYZ = glm::vec3(1.8f, 13.6f, -5.90f);         // Middle right tile
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // Opposite of 270°
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Mirror Frame: Right Tile 3 **//
	positionXYZ = glm::vec3(1.8f, 12.4f, -5.90f);         // Bottom right vertical tile
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 90.0f, positionXYZ);  // Consistent finish
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** MIRROR FRAME: BOTTOM ROW **//
//******************************//

	//** Mirror Frame: Bottom Left Tile **//
	positionXYZ = glm::vec3(-0.6f, 12.4f, -5.90f);        // Bottom left horizontal
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 90.0f, positionXYZ);  // Match top mid-left
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Mirror Frame: Bottom Right Tile **//
	positionXYZ = glm::vec3(0.6f, 12.4f, -5.90f);         // Bottom right horizontal
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 180.0f, positionXYZ);  // Match top mid-right
	SetShaderMaterial("zebraMat");
	SetShaderTexture("ZebraFur");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** MIRROR: Inner Reflective Surface **//
//*************************************//
	scaleXYZ = glm::vec3(2.4f, 2.4f, 0.1f);               // Large central square, slightly thinner depth
	positionXYZ = glm::vec3(0.0f, 14.2f, -5.92f);         // Slightly inlaid inside the frame
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("mirrorMat");                      // Glossy reflective surface
	SetShaderTexture("Mirror");                          // Smooth silver mirror image
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** CABINET: SUPPORT PEGS **//
	//***************************//

	scaleXYZ = glm::vec3(0.4f, 1.0f, 0.4f);               // Short wide pegs, 1/5 cabinet height

	//** Front Left Peg **//
	positionXYZ = glm::vec3(-2.8f, 0.5f, -0.7f);          // Front left corner under cabinet
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black color (reacts with lighting)
	m_basicMeshes->DrawBoxMesh();

	//** Front Right Peg **//
	positionXYZ = glm::vec3(2.8f, 0.5f, -0.7f);           // Front right corner under cabinet
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Back Left Peg **//
	positionXYZ = glm::vec3(-2.8f, 0.5f, -5.3f);          // Back left corner under cabinet
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Back Right Peg **//
	positionXYZ = glm::vec3(2.8f, 0.5f, -5.3f);           // Back right corner under cabinet
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** CABINET: MAIN BODY **//
//*************************//

	scaleXYZ = glm::vec3(6.0f, 4.5f, 5.0f);                 // Wider cabinet body with reduced height
	positionXYZ = glm::vec3(0.0f, 3.25f, -3.0f);             // Centered and flush on top of 1.0f tall pegs
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("mirrorMat");                        // Glossy reflective surface
	SetShaderTexture("Mirror");                            // Mirror texture for cabinet body
	SetTextureUVScale(1.0f, 1.0f);                          // Standard texture scale
	m_basicMeshes->DrawBoxMesh();

//** CABINET: TOP MIRROR PANEL **//
//*******************************//

	scaleXYZ = glm::vec3(5.6f, 0.325f, 4.6f);                // Thin panel, slightly inset from cabinet body
	positionXYZ = glm::vec3(0.0f, 5.66f, -3.0f);           // Sits flush on top of 4.5f tall cabinet body
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("mirrorMat");                       // Glossy reflective surface
	SetShaderTexture("Mirror");                           // Same mirror texture as cabinet body
	SetTextureUVScale(1.0f, 1.0f);                         // Standard texture scale
	m_basicMeshes->DrawBoxMesh();

//** CABINET: TOP OVERHANG STRIPS **//
//**********************************//

	//** Front Overhang Strip **//
	//**************************//

	scaleXYZ = glm::vec3(6.4f, 0.325f, 0.5f);             // Slightly wider than cabinet, thin height, front depth
	positionXYZ = glm::vec3(0.0f, 5.66f, -0.445f);        // Aligned with front edge of cabinet
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);          // Solid black color (reflects light)
	m_basicMeshes->DrawBoxMesh();

	//** Back Overhang Strip **//
	//*************************//

	scaleXYZ = glm::vec3(6.4f, 0.325f, 0.5f);             // Matches front strip: wide, thin, deep
	positionXYZ = glm::vec3(0.0f, 5.66f, -5.55f);         // Aligned with back edge of cabinet
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black color (reflects light)
	m_basicMeshes->DrawBoxMesh();

	//** Left Overhang Strip **//
	//**************************//

	scaleXYZ = glm::vec3(0.41f, 0.325f, 4.7f);             // Narrow width, thin height, matches cabinet depth
	positionXYZ = glm::vec3(-3.0f, 5.66f, -3.0f);         // Aligned with left edge, flush with top mirror panel
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black color (reflects light)
	m_basicMeshes->DrawBoxMesh();

	//** Right Overhang Strip **//
	//***************************//

	positionXYZ = glm::vec3(3.0f, 5.66f, -3.0f);          // Aligned with right edge, same depth
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** CABINET: FRONT EDGE PANELS (THIN BLACK)**//
//********************************************//

	//** Left Edge Strip **//
	//*********************//

	scaleXYZ = glm::vec3(0.2f, 4.5f, 0.5f);               // Thin strip, matches cabinet height
	positionXYZ = glm::vec3(-3.0f, 3.25f, -0.65f);       // Aligned left, flush with mirror front
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Right Edge Strip **//
	//**********************//

	positionXYZ = glm::vec3(3.0f, 3.25f, -0.65f);        // Aligned right, mirror depth
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Bottom Edge Strip **//
	//***********************//
	scaleXYZ = glm::vec3(6.0f, 0.2f, 0.1f);               // Wide horizontal strip, matches cabinet width
	positionXYZ = glm::vec3(0.0f, 1.1f, -0.45f);          // Aligned bottom, flush under mirror
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** CABINET: RIGHT SIDE EDGE STRIPS **//
//********************************//

	//** Right Top Edge Strip **//
	//**************************//

	scaleXYZ = glm::vec3(0.2f, 0.5f, 4.2f);              // Thin horizontal strip on top right edge
	positionXYZ = glm::vec3(3.0f, 5.3f, -3.0f);           // Matches height and depth of top panel
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);             // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Right Back Edge Strip **//
	//***************************//

	scaleXYZ = glm::vec3(0.2f, 4.5f, 0.5f);               // Tall vertical strip, flush against right side
	positionXYZ = glm::vec3(3.0f, 3.25f, -5.35f);         // Same Y/Z as left edge strip
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Right Bottom Strip **//
	//************************//

	scaleXYZ = glm::vec3(0.2f, 0.5f, 4.2f);               // Small horizontal strip along bottom edge
	positionXYZ = glm::vec3(3.0f, 1.25f, -3.0f);          // Flush with right base, matches bottom strip
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** CABINET: LEFT SIDE EDGE STRIPS **//
//************************************//

	//** Left Top Edge Strip **//
	//*************************//

	scaleXYZ = glm::vec3(0.2f, 0.5f, 4.2f);              // Thin horizontal strip on top left edge
	positionXYZ = glm::vec3(-3.0f, 5.3f, -3.0f);         // Matches height and depth of top panel
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);           // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Left Back Edge Strip **//
	//**************************//

	scaleXYZ = glm::vec3(0.2f, 4.5f, 0.5f);              // Tall vertical strip, flush against left side
	positionXYZ = glm::vec3(-3.0f, 3.25f, -5.35f);       // Matches right back edge strip
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Left Bottom Strip **//
	//***********************//

	scaleXYZ = glm::vec3(0.2f, 0.5f, 4.2f);              // Small horizontal strip along bottom edge
	positionXYZ = glm::vec3(-3.0f, 1.25f, -3.0f);        // Flush with left base, matches bottom strip
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** CABINET: FRONT VERTICAL FRAME STRIPS **//
//******************************************//

	//** Front Left Door Frame Strip **//
	//*********************************//

	scaleXYZ = glm::vec3(0.2f, 4.3f, 0.1f);              // Thin, tall, shallow strip for door frame
	positionXYZ = glm::vec3(-2.2f, 3.35f, -0.45f);      // Aligned to left edge, flush with front
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);           // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Front Right Door Frame Strip **//
	//**********************************//

	positionXYZ = glm::vec3(2.2f, 3.35f, -0.45f);       // Aligned to right edge, flush with front
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** CABINET: FRONT HORIZONTAL FRAME STRIPS **//
//********************************************//

	//** Top Door Frame Strip **//
	//**************************//

	scaleXYZ = glm::vec3(4.2f, 0.2f, 0.1f);             // Thin horizontal strip, spans width of door section
	positionXYZ = glm::vec3(0.0f, 5.1f, -0.45f);        // Positioned just below the top panel
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);          // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Bottom Door Frame Strip **//
	//*****************************//

	positionXYZ = glm::vec3(0.0f, 1.6f, -0.45f);        // Positioned just above the bottom panel
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** CABINET: DOOR OUTER TRIM PIECES **//
//**************************************//

	// ===== LEFT DOOR ===== //

	//** Left Door - Left Trim **//
	//***************************//

	scaleXYZ = glm::vec3(0.3f, 2.7f, 0.1f);
	positionXYZ = glm::vec3(-2.0f, 3.35f, -0.45f);         // Left edge of left door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Left Door - Right Trim **//
	//****************************//

	positionXYZ = glm::vec3(-0.1f, 3.35f, -0.45f);         // Right edge of left door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Left Door - Top Trim **//
	//**************************//

	scaleXYZ = glm::vec3(2.2f, 0.3f, 0.1f);               // Horizontal top trim
	positionXYZ = glm::vec3(-1.1f, 4.84f, -0.45f);          // Top of left door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Left Door - Bottom Trim **//
	//*****************************//

	positionXYZ = glm::vec3(-1.1f, 1.86f, -0.45f);          // Bottom of left door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();


	// ===== RIGHT DOOR ===== //

	//** Right Door - Left Trim **//
	//****************************//

	scaleXYZ = glm::vec3(0.3f, 2.7f, 0.1f);
	positionXYZ = glm::vec3(0.1f, 3.35f, -0.45f);         // Left edge of right door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Right Door - Right Trim **//
	//*****************************//

	positionXYZ = glm::vec3(2.0f, 3.35f, -0.45f);         // Right edge of right door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Right Door - Top Trim **//
	//***************************//

	scaleXYZ = glm::vec3(2.2f, 0.3f, 0.1f);               // Horizontal top trim
	positionXYZ = glm::vec3(1.1f, 4.84f, -0.45f);          // Top of right door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Right Door - Bottom Trim **//
	//******************************//

	positionXYZ = glm::vec3(1.1f, 1.86f, -0.45f);          // Bottom of right door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** CABINET: DOOR RING **//
//********************************//

//** Left Door Ring Handle **//
	scaleXYZ = glm::vec3(0.4f, 0.4f, 0.25f);             // Larger diameter
	positionXYZ = glm::vec3(-1.1f, 3.35f, -0.45f);        // Slightly forward from door face
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black ring
	m_basicMeshes->DrawTorusMesh();

	//** Right Door Ring **//
	positionXYZ = glm::vec3(1.1f, 3.35f, -0.45f);         // Mirror position for right door
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawTorusMesh();

//** CABINET: DOOR HANDLE CONNECTORS **//
//*************************************//

	// ===== LEFT DOOR ===== //

	//** Left Door - Vertical Connector (Top) **//
	//******************************************//

	scaleXYZ = glm::vec3(0.2f, 1.1f, 0.1f);              // Thin vertical bar above/below circle
	positionXYZ = glm::vec3(-1.1f, 4.3f, -0.45f);        // Aligned vertically with circle
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Left Door - Vertical Connector (Bottom) **//
	//******************************************//

	positionXYZ = glm::vec3(-1.1f, 2.4f, -0.45f);        // Aligned vertically with circle
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Left Door - Horizontal Connector (Left) **//
	//*********************************************//

	scaleXYZ = glm::vec3(0.4f, 0.2f, 0.1f);               // Thin horizontal bar left/right of circle
	positionXYZ = glm::vec3(-1.75f, 3.35f, -0.45f);        // Same center point as vertical
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Left Door - Horizontal Connector (Right) **//
	//*********************************************//

	positionXYZ = glm::vec3(-0.45f, 3.35f, -0.45f);        // Same center point as vertical
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();


	// ===== RIGHT DOOR ===== //

	//** Right Door - Vertical Connector (Top) **//
	//******************************************//

	scaleXYZ = glm::vec3(0.2f, 1.1f, 0.1f);              // Thin vertical bar above/below circle
	positionXYZ = glm::vec3(1.1f, 4.3f, -0.45f);        // Aligned vertically with circle
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Right Door - Vertical Connector (Bottom) **//
	//******************************************//

	positionXYZ = glm::vec3(1.1f, 2.4f, -0.45f);        // Aligned vertically with circle
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);            // Solid black (reacts with light)
	m_basicMeshes->DrawBoxMesh();

	//** Right Door - Horizontal Connector (Right) **//
	//*********************************************//

	scaleXYZ = glm::vec3(0.4f, 0.2f, 0.1f);               // Thin horizontal bar left/right of circle
	positionXYZ = glm::vec3(1.75f, 3.35f, -0.45f);        // Same center point as vertical
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	//** Right Door - Horizontal Connector (Left) **//
	//*********************************************//

	positionXYZ = glm::vec3(0.45f, 3.35f, -0.45f);        // Same center point as vertical
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.05f, 0.05f, 0.05f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

//** Left Door - Handle Base Plate (Stacked Pyramids)**//
//****************************************************//

	//** Upper Pyramid (Flipped) **//
	//*****************************//

	scaleXYZ = glm::vec3(0.15f, 0.15f, 0.15f);              // Same size
	positionXYZ = glm::vec3(-0.125f, 3.30f, -0.435f);         // Slightly above the lower pyramid
	SetTransformations(scaleXYZ, 20.0f, 0.0f, 180.0f, positionXYZ);  // Flipped upside-down
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);               // Same gold tone
	m_basicMeshes->DrawPyramid3Mesh();

	//** Lower Pyramid **//
	//*******************//

	scaleXYZ = glm::vec3(0.15f, 0.15f, 0.15f);             // Same size as upper
	positionXYZ = glm::vec3(-0.125f, 3.35f, -0.435f);        // Bottom half of base plate
	SetTransformations(scaleXYZ, 20.0f, 0.0f, 0.0f, positionXYZ);    // Regular orientation
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);
	m_basicMeshes->DrawPyramid3Mesh();

//** Right Door - Handle Base Plate (Stacked Pyramids)**//
//****************************************************//

	//** Upper Pyramid (Flipped) - RIGHT **//
	//*************************************//

	scaleXYZ = glm::vec3(0.15f, 0.15f, 0.15f);              // Same size
	positionXYZ = glm::vec3(0.125f, 3.30f, -0.435f);        // Mirrored X from left side
	SetTransformations(scaleXYZ, 20.0f, 0.0f, 180.0f, positionXYZ);  // Flipped upside-down
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);               // Same gold tone
	m_basicMeshes->DrawPyramid3Mesh();

	//** Lower Pyramid - RIGHT **//
	//***************************//

	scaleXYZ = glm::vec3(0.15f, 0.15f, 0.15f);              // Same size
	positionXYZ = glm::vec3(0.125f, 3.35f, -0.435f);        // Mirrored X, slightly higher Y
	SetTransformations(scaleXYZ, 20.0f, 0.0f, 0.0f, positionXYZ);    // Regular orientation
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);
	m_basicMeshes->DrawPyramid3Mesh();

//** Gold Spheres Ornate - LEFT Base Plate **//
//**********************************************//

	scaleXYZ = glm::vec3(0.02f, 0.02f, 0.02f);             // Small sphere size
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);               // Same gold tone

	// Top-Left
	positionXYZ = glm::vec3(-0.185f, 3.39f, -0.40f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Top-Right
	positionXYZ = glm::vec3(-0.06f, 3.39f, -0.40f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Bottom-Left
	positionXYZ = glm::vec3(-0.19f, 3.26f, -0.40f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Bottom-Right
	positionXYZ = glm::vec3(-0.06f, 3.26f, -0.40f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

//** Gold Spheres Ornate - RIGHT Base Plate **//
//***********************************************//

	scaleXYZ = glm::vec3(0.02f, 0.02f, 0.02f);             // Small sphere size
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);               // Same gold tone

	// Top-Left
	positionXYZ = glm::vec3(0.06f, 3.39f, -0.40f);          // Upper left corner of base plate
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Top-Right
	positionXYZ = glm::vec3(0.185f, 3.39f, -0.40f);         // Upper right corner of base plate
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Bottom-Left
	positionXYZ = glm::vec3(0.06f, 3.26f, -0.40f);          // Lower left corner of base plate
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Bottom-Right
	positionXYZ = glm::vec3(0.185f, 3.26f, -0.40f);         // Lower right corner of base plate
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

//** CABINET HANDLE: LEFT CLASP ARMS **//
//****************************************//

// Left Arm – Cylinder + Cap (Left of base) //
//******************************************//

	//** Arm Cylinder (Left Side) **//
	//******************************//

	scaleXYZ = glm::vec3(0.005f, 0.03f, 0.005f);            // Short, thin protrusion
	positionXYZ = glm::vec3(-0.13f, 3.33f, -0.39f);          // Left side of base plate
	SetTransformations(scaleXYZ, 90.0f, 0.0f, 0.0f, positionXYZ);  // Point outward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawCylinderMesh();

	//** Left Torus End Cap (Left Side) **//
	//************************************//

	scaleXYZ = glm::vec3(0.005f, 0.005f, 0.005f);            // Ring size and thickness
	positionXYZ = glm::vec3(-0.13f, 3.33f, -0.36f);          // Positioned at cylinder tip
	SetTransformations(scaleXYZ, 180.0f, 90.0f, 0.0f, positionXYZ);  // Face forward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawTorusMesh();

	//** Inner Sphere Connector (Left Side) **//
	//****************************************//

	scaleXYZ = glm::vec3(0.0055f, 0.0055f, 0.0055f);         // Fills torus center
	positionXYZ = glm::vec3(-0.13f, 3.33f, -0.36f);          // Same as torus
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawSphereMesh();


	// Right Arm – Cylinder + Cap (Right of base) //
	//********************************************//

		//** Arm Cylinder (Right Side) **//
		//*******************************//

	scaleXYZ = glm::vec3(0.005f, 0.03f, 0.005f);             // Short, thin protrusion
	positionXYZ = glm::vec3(-0.12f, 3.33f, -0.39f);          // Right side of base plate
	SetTransformations(scaleXYZ, 90.0f, 0.0f, 0.0f, positionXYZ);  // Point outward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawCylinderMesh();

	//** Right Torus End Cap (Right Side) **//
	//**************************************//

	scaleXYZ = glm::vec3(0.005f, 0.005f, 0.005f);            // Ring size and thickness
	positionXYZ = glm::vec3(-0.12f, 3.33f, -0.36f);          // Positioned at cylinder tip
	SetTransformations(scaleXYZ, 180.0f, 90.0f, 0.0f, positionXYZ);  // Face forward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawTorusMesh();

	//** Inner Sphere Connector (Right Side) **//
	//*****************************************//

	scaleXYZ = glm::vec3(0.0055f, 0.0055f, 0.0055f);         // Fills torus center
	positionXYZ = glm::vec3(-0.12f, 3.33f, -0.36f);          // Same as torus
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawSphereMesh();

	//** Hanging Torus Between Clasps **//
	//**********************************//

	scaleXYZ = glm::vec3(0.0060f, 0.0060f, 0.0060f);       // Slightly larger than side rings
	positionXYZ = glm::vec3(-0.125f, 3.328f, -0.36f);      // Centered between arms, slightly lower and forward
	SetTransformations(scaleXYZ, 180.0f, 90.0f, 0.0f, positionXYZ);  // Rotated to face outward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);              // Same gold tone
	m_basicMeshes->DrawTorusMesh();

	//** Hanging Handle – Flattened Tapered Cylinder **//
	//*************************************************//

	scaleXYZ = glm::vec3(0.02f, 0.11f, 0.000001f);         // Tall and flat with nearly invisible depth
	positionXYZ = glm::vec3(-0.125f, 3.215f, -0.36f);     // Aligned just below torus loop
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation, hangs straight down
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);             // Consistent gold finish
	m_basicMeshes->DrawTaperedCylinderMesh();            // Flat tapered shape completes pendant look

// Bottom Handle Cap – Cylinder + End Spheres //
//********************************************//

	//** Bottom Bar (Horizontal Cylinder) **//
	//**************************************//

	scaleXYZ = glm::vec3(0.004f, 0.025f, 0.004f);         // Thin and short, stretching across the base
	positionXYZ = glm::vec3(-0.112f, 3.22f, -0.356f);      // Just below the handle’s lower tip
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 90.0f, positionXYZ);  // Rotate horizontally across X
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);              // Same gold tone
	m_basicMeshes->DrawCylinderMesh();

	//** Left End Cap (Sphere) **//
	//***************************//

	scaleXYZ = glm::vec3(0.0045f, 0.0045f, 0.0045f);          // Small ball at end of the bar
	positionXYZ = glm::vec3(-0.138f, 3.22f, -0.356f);      // Left tip of the bar
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation needed
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);              // Match handle tone
	m_basicMeshes->DrawSphereMesh();

	//** Right End Cap (Sphere) **//
	//****************************//

	scaleXYZ = glm::vec3(0.0045f, 0.0045f, 0.0045f);          // Same size as left
	positionXYZ = glm::vec3(-0.114f, 3.22f, -0.356f);      // Right tip of the bar
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation needed
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);              // Gold tone
	m_basicMeshes->DrawSphereMesh();

//** CABINET HANDLE: RIGHT CLASP ARMS **//
//****************************************//

// Right Arm – Cylinder + Cap (Right of base) //
//********************************************//

	//** Arm Cylinder (Right Side) **//
	//*******************************//

	scaleXYZ = glm::vec3(0.005f, 0.03f, 0.005f);             // Short, thin protrusion
	positionXYZ = glm::vec3(0.13f, 3.33f, -0.39f);           // Right side of base plate
	SetTransformations(scaleXYZ, 90.0f, 0.0f, 0.0f, positionXYZ);  // Point outward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                 // Gold material
	m_basicMeshes->DrawCylinderMesh();

	//** Right Torus End Cap (Right Side) **//
	//**************************************//

	scaleXYZ = glm::vec3(0.005f, 0.005f, 0.005f);            // Ring size and thickness
	positionXYZ = glm::vec3(0.13f, 3.33f, -0.36f);           // Positioned at cylinder tip
	SetTransformations(scaleXYZ, 180.0f, 90.0f, 0.0f, positionXYZ);  // Face forward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawTorusMesh();

	//** Inner Sphere Connector (Right Side) **//
	//*****************************************//

	scaleXYZ = glm::vec3(0.0055f, 0.0055f, 0.0055f);         // Fills torus center
	positionXYZ = glm::vec3(0.13f, 3.33f, -0.36f);           // Same as torus
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawSphereMesh();


// Left Arm – Cylinder + Cap (Left of base) //
//******************************************//

	//** Arm Cylinder (Left Side) **//
	//******************************//

	scaleXYZ = glm::vec3(0.005f, 0.03f, 0.005f);             // Short, thin protrusion
	positionXYZ = glm::vec3(0.12f, 3.33f, -0.39f);           // Left side of base plate
	SetTransformations(scaleXYZ, 90.0f, 0.0f, 0.0f, positionXYZ);  // Point outward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawCylinderMesh();

	//** Left Torus End Cap (Left Side) **//
	//************************************//

	scaleXYZ = glm::vec3(0.005f, 0.005f, 0.005f);            // Ring size and thickness
	positionXYZ = glm::vec3(0.12f, 3.33f, -0.36f);           // Positioned at cylinder tip
	SetTransformations(scaleXYZ, 180.0f, 90.0f, 0.0f, positionXYZ);  // Face forward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawTorusMesh();

	//** Inner Sphere Connector (Left Side) **//
	//****************************************//

	scaleXYZ = glm::vec3(0.0055f, 0.0055f, 0.0055f);         // Fills torus center
	positionXYZ = glm::vec3(0.12f, 3.33f, -0.36f);           // Same as torus
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold material
	m_basicMeshes->DrawSphereMesh();

	//** Hanging Torus Between Clasps **//
	//**********************************//

	scaleXYZ = glm::vec3(0.0060f, 0.0060f, 0.0060f);         // Slightly larger than side rings
	positionXYZ = glm::vec3(0.125f, 3.328f, -0.36f);         // Centered between arms, slightly lower and forward
	SetTransformations(scaleXYZ, 180.0f, 90.0f, 0.0f, positionXYZ);  // Rotated to face outward
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Same gold tone
	m_basicMeshes->DrawTorusMesh();

	//** Hanging Handle – Flattened Tapered Cylinder **//
	//*************************************************//

	scaleXYZ = glm::vec3(0.02f, 0.11f, 0.000001f);           // Tall and flat with nearly invisible depth
	positionXYZ = glm::vec3(0.125f, 3.215f, -0.36f);         // Aligned just below torus loop
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation, hangs straight down
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Consistent gold finish
	m_basicMeshes->DrawTaperedCylinderMesh();               // Flat tapered shape completes pendant look

// Bottom Handle Cap – Cylinder + End Spheres //
//********************************************//

	//** Bottom Bar (Horizontal Cylinder) **//
	//**************************************//

	scaleXYZ = glm::vec3(0.004f, 0.025f, 0.004f);            // Thin and short, stretching across the base
	positionXYZ = glm::vec3(0.14f, 3.22f, -0.356f);         // Just below the handle’s lower tip
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 90.0f, positionXYZ);  // Rotate horizontally across X
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Same gold tone
	m_basicMeshes->DrawCylinderMesh();

	//** Left End Cap (Sphere) **//
	//***************************//

	scaleXYZ = glm::vec3(0.0045f, 0.0045f, 0.0045f);         // Small ball at end of the bar
	positionXYZ = glm::vec3(0.138f, 3.22f, -0.356f);         // Left tip of the bar
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation needed
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Match handle tone
	m_basicMeshes->DrawSphereMesh();

	//** Right End Cap (Sphere) **//
	//****************************//

	scaleXYZ = glm::vec3(0.0045f, 0.0045f, 0.0045f);         // Same size as left
	positionXYZ = glm::vec3(0.114f, 3.22f, -0.356f);         // Right tip of the bar
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation needed
	SetShaderColor(0.85f, 0.65f, 0.2f, 1.0f);                // Gold tone
	m_basicMeshes->DrawSphereMesh();

//** Decorative Box **//
//********************//

	//** BOX BASE (Chevron Sides) **//
	//******************************//

	scaleXYZ = glm::vec3(3.6f, 1.3f, 3.0f);                // Slightly smaller than cabinet top, centered
	positionXYZ = glm::vec3(0.0f, 6.45f, -3.5f);           // Sits directly above the mirror panel (cabinet top)
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("chevronMat");                      // Stitched fur with soft lighting
	SetShaderTexture("ChevronFur");                       // Chevron pattern fur texture
	SetTextureUVScale(2.0f, 1.0f);                         // Double tiling scaling
	m_basicMeshes->DrawBoxMesh();


	//** BOX LID (Flat Fur Top) **//
	//****************************//

	scaleXYZ = glm::vec3(3.6f, 0.3f, 3.0f);                // Thin lid layer to sit on top of box base
	positionXYZ = glm::vec3(0.0f, 7.25f, -3.5f);            // Slightly above the box base 
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("boxFurMat");                        // Solid fur material with soft sheen
	SetShaderTexture("BoxFur");                            // Matching grey fur texture
	SetTextureUVScale(1.0f, 1.0f);                          // Standard texture scale
	m_basicMeshes->DrawBoxMesh();

//** LAMP BASE **//
//***************//

 //** Cylinder 1: Base layer **//
//****************************//
	scaleXYZ = glm::vec3(1.15f, 0.1f, 1.15f);       // Wide and flat for a strong base
	positionXYZ = glm::vec3(0.0f,7.35f, -3.5f);    // Slightly lifted above the floor
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // No rotation

	SetShaderMaterial("copper");          // copper material (lighting)
	SetShaderTexture("Copper");           // copper texture
	SetTextureUVScale(1.6f, 1.6f);        // UV scale
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // neutral tint (preserve texture color)
	m_basicMeshes->DrawCylinderMesh();


	//** Cylinder 2: Main platform **//
	//*******************************//
	scaleXYZ = glm::vec3(1.1f, 0.3f, 1.1f);
	positionXYZ = glm::vec3(0.0f, 7.41f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderMaterial("copper");          // copper material (lighting)
	// Keep texture visible but avoid looking identical to Cylinder 1
	SetShaderTexture("Copper");
	SetTextureUVScale(1.4f, 1.4f);                 // slight variation reduces pattern repetition banding
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawCylinderMesh();


	//** Cylinder 3: Upper platform **//
	//*********************************//
	scaleXYZ = glm::vec3(0.9f, 0.1f, 0.9f);
	positionXYZ = glm::vec3(0.0f, 7.71f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderMaterial("copper");          // copper material (lighting)
	// Slightly tighter to keep the top ring crisp
	SetShaderTexture("Copper");
	SetTextureUVScale(1.7f, 1.7f);                 // a touch tighter for a crisp edge read
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawCylinderMesh();


	//** Pipe base **//
	//***************//
	scaleXYZ = glm::vec3(0.625f, 0.1f, 0.625f);
	positionXYZ = glm::vec3(0.0f, 7.80f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderMaterial("copper");          // copper material (lighting)
	// Pipes look better with tighter brush detail
	SetShaderTexture("Copper");
	SetTextureUVScale(1.8f, 1.8f);                 // tighter grain helps smaller parts read as metal
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawCylinderMesh();


	//** Torus connector (bottom) **//
	//******************************//
	scaleXYZ = glm::vec3(0.55f, 0.55f, 0.55f);
	positionXYZ = glm::vec3(0.0f, 7.94f, -3.5f);
	SetTransformations(scaleXYZ, 90.0f, 0.0f, 0.0f, positionXYZ);  // ring alignment

	SetShaderMaterial("copper");          // copper material (lighting)
	// Slightly different UV to break repetition on circular forms
	SetShaderTexture("Copper");
	SetTextureUVScale(1.5f, 1.5f);                 // keeps the ring from showing obvious repeats
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawTorusMesh();


	//** Segment 2: Thin ring **//
	//**************************//
	scaleXYZ = glm::vec3(0.56f, 0.05f, 0.56f);
	positionXYZ = glm::vec3(0.0f, 8.05f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderMaterial("copper");          // copper material (lighting)
	// Thin parts get a tighter UV so the texture doesn’t blur
	SetShaderTexture("Copper");
	SetTextureUVScale(2.0f, 2.0f);                 // higher = finer detail, avoids muddy look on thin rings
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawCylinderMesh();


	//** Segment 3: Narrow section **//
	//*******************************//
	scaleXYZ = glm::vec3(0.4f, 0.20f, 0.4f);
	positionXYZ = glm::vec3(0.0f, 8.10f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderMaterial("copper");          // copper material (lighting)
	// Keep the pipe crisp
	SetShaderTexture("Copper");
	SetTextureUVScale(1.9f, 1.9f);
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawCylinderMesh();


	//** Segment 4: Mid-section bulge **//
	//**********************************//
	scaleXYZ = glm::vec3(0.50f, 0.20f, 0.50f);
	positionXYZ = glm::vec3(0.0f, 8.30f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderMaterial("copper");          // copper material (lighting)
	// Slight variation keeps the eye from spotting repeats
	SetShaderTexture("Copper");
	SetTextureUVScale(1.6f, 1.6f);
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawCylinderMesh();


	//** Torus connector (mid) **//
	//***************************//
	scaleXYZ = glm::vec3(0.35f, 0.35f, 0.35f);
	positionXYZ = glm::vec3(0.0f, 8.55f, -3.5f);
	SetTransformations(scaleXYZ, 90.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderMaterial("copper");          // copper material (lighting)
	// Match lower ring but not identical
	SetShaderTexture("Copper");
	SetTextureUVScale(1.7f, 1.7f);
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawTorusMesh();


	//** Segment 6: Main section **//
	//*****************************//
	scaleXYZ = glm::vec3(0.6f, 0.20f, 0.6f);
	positionXYZ = glm::vec3(0.0f, 8.60f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderMaterial("copper");          // copper material (lighting)
	// Keep the mid body consistent with earlier pipes
	SetShaderTexture("Copper");
	SetTextureUVScale(1.8f, 1.8f);
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawCylinderMesh();


	//** Segment 7: Slender neck **//
	//*****************************//
	scaleXYZ = glm::vec3(0.40f, 0.20f, 0.40f);
	positionXYZ = glm::vec3(0.0f, 8.8f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);  // Upright with no rotation

	SetShaderMaterial("copper");          // copper material (lighting)
	// Tightest grain on the thinnest piece to avoid blur
	SetShaderTexture("Copper");
	SetTextureUVScale(2.2f, 2.2f);                 // thinnest piece -> tightest UV for clarity
	m_pShaderManager->setVec4Value(g_ColorValueName, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_basicMeshes->DrawCylinderMesh();


	//** Segment 8: Tall connector pipe **//
	//************************************//
	scaleXYZ = glm::vec3(0.12f, 0.45f, 0.12f);     // Long and skinny to create height
	positionXYZ = glm::vec3(0.0f, 8.9f, -3.5f);     // Extends from the neck vertically
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.0f, 0.0f, 0.0f, 1.0f);        // Solid black for a plastic-like finish
	SetShaderMaterial("plasticBlack");    // plastic material (lighting)
	m_basicMeshes->DrawCylinderMesh();

	//** Torus band (plastic connector) **//
	//************************************//
	scaleXYZ = glm::vec3(0.15f, 0.15f, 0.15f);     // Small and flush to wrap around pipe
	positionXYZ = glm::vec3(0.0f, 9.35f, -3.5f);    // Sits at the top of the tall pipe
	SetTransformations(scaleXYZ, 90.0f, 0.0f, 0.0f, positionXYZ);  // Rotated for horizontal alignment
	SetShaderColor(0.0f, 0.0f, 0.0f, 1.0f);        // Matches the plastic connector pipe
	SetShaderMaterial("plasticBlack");    // plastic material (lighting)
	m_basicMeshes->DrawTorusMesh();

	//** Segment 9: Plastic connector pipe **//
	//***************************************//
	scaleXYZ = glm::vec3(0.18f, 0.2f, 0.18f);      // Slightly thicker than the previous pipe
	positionXYZ = glm::vec3(0.0f, 9.35f, -3.5f);    // Overlaps the torus directly
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.0f, 0.0f, 0.0f, 1.0f);        // Black to keep it consistent with other segments
	SetShaderMaterial("plasticBlack");    // plastic material (lighting)
	m_basicMeshes->DrawCylinderMesh();

	//** Segment 10: Short cap segment **//
	//***********************************//
	scaleXYZ = glm::vec3(0.22f, 0.1f, 0.22f);      // Wider but shallow to act as a top cap
	positionXYZ = glm::vec3(0.0f, 9.50f, -3.5f);     // Stacked above segment 9
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.0f, 0.0f, 0.0f, 1.0f);        // Black again for plastic cohesion
	SetShaderMaterial("plasticBlack");    // plastic material (lighting)
	m_basicMeshes->DrawCylinderMesh();

	//** Segment 11: Top plastic piece **//
	//***********************************//
	scaleXYZ = glm::vec3(0.15f, 0.2f, 0.15f);      // Narrower and taller than the cap
	positionXYZ = glm::vec3(0.0f, 9.60f, -3.5f);     // Sits directly on Segment 10
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.0f, 0.0f, 0.0f, 1.0f);        // Consistent black plastic color
	SetShaderMaterial("plasticBlack");    // plastic material (lighting)
	m_basicMeshes->DrawCylinderMesh();

	//** Bulb neck: transition to glass **//
	//************************************//
	scaleXYZ = glm::vec3(0.08f, 0.06f, 0.08f);     // Small and subtle for transition
	positionXYZ = glm::vec3(0.0f, 9.8f, -3.5f);     // Starts to lead into the bulb
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.4f, 0.4f, 0.4f, 1.0f);        // Neutral gray to separate plastic and glass
	m_basicMeshes->DrawCylinderMesh();

	//** Glass bulb (emissive) **//
    //***************************//
	scaleXYZ = glm::vec3(0.22f, 0.32f, 0.22f);   // bulb size
	positionXYZ = glm::vec3(0.0f,10.15f, -3.5f);     // bulb position
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	m_pShaderManager->setBoolValue("bUseLighting", false);  // emissive draw
	SetShaderColor(1.3f, 1.1f, 0.65f, 1.0f);              // warm yellow (not pure white)
	m_basicMeshes->DrawSphereMesh();                        // draw bulb
	m_pShaderManager->setBoolValue("bUseLighting", true);   // restore lighting

	//** Switch stem: horizontal toggle arm **//
	//****************************************//
	scaleXYZ = glm::vec3(0.03f, 0.8f, 0.03f);      // Very slim and long to reach out
	positionXYZ = glm::vec3(0.9f, 9.55f, -3.5f);    // Positioned off the side
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 90.0f, positionXYZ);  // Rotated to extend sideways
	SetShaderColor(0.0f, 0.0f, 0.0f, 1.0f);        // Matches the black plastic segments
	SetShaderMaterial("plasticBlack");    // plastic material (lighting)
	m_basicMeshes->DrawCylinderMesh();

	//** Switch cap: knob at the end **//
	//*********************************//
	scaleXYZ = glm::vec3(0.1f, 0.05f, 0.1f);       // Slightly larger round end
	positionXYZ = glm::vec3(0.95f, 9.55f, -3.5f);   // Aligned with the end of the stem
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 90.0f, positionXYZ);  // Same orientation as stem
	SetShaderColor(0.1f, 0.1f, 0.1f, 1.0f);        // Slightly lighter to stand out a bit
	SetShaderMaterial("plasticBlack");    // plastic material (lighting)
	m_basicMeshes->DrawCylinderMesh();

	// ================= GLASS SHADE ==================
// switch to blended rendering for glass
	glEnable(GL_BLEND);                                            // translucent pass
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);             // standard alpha blend
	glDepthMask(GL_FALSE);                                         // no depth writes while blending
	glEnable(GL_CULL_FACE);                                        // cull for inside/outside faces

	const glm::vec4 kGlassRGBA = glm::vec4(0.85f, 0.90f, 1.0f, 0.38f); // shared glass tint

	//** Inner taper (glass funnel) **//
	//********************************//
	glCullFace(GL_BACK);
	scaleXYZ = glm::vec3(0.30f, -1.34f, 0.30f);                 // slightly taller to meet cylinder
	positionXYZ = glm::vec3(0.0f, 9.46f, -3.5f);                    // nudge up to kiss inner wall
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("glass");
	SetShaderTexture("FrostedGlass");
	SetTextureUVScale(1.4f, 1.4f);
	m_pShaderManager->setVec4Value(g_ColorValueName, kGlassRGBA);
	m_basicMeshes->DrawTaperedCylinderMesh();

	//** Inner glass cylinder (nearly flush) **//
	//*****************************************//
	// draw back faces so we see the interior wall
	glCullFace(GL_FRONT);
	scaleXYZ = glm::vec3(0.796f, 2.52f, 0.796f);                // radius close to outer (clearance ~0.004)
	positionXYZ = glm::vec3(0.0f, 9.40f, -3.5f);                     // same center as outer
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("glass");
	SetShaderTexture("FrostedGlass");
	SetTextureUVScale(1.1f, 1.1f);
	m_pShaderManager->setVec4Value(g_ColorValueName, kGlassRGBA);
	m_basicMeshes->DrawCylinderMesh();

	//** Outer glass cylinder **//
	//**************************//
	glCullFace(GL_BACK);
	scaleXYZ = glm::vec3(0.800f, 2.50f, 0.800f);                // outer radius
	positionXYZ = glm::vec3(0.0f, 9.40f, -3.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("glass");
	SetShaderTexture("FrostedGlass");
	SetTextureUVScale(1.2f, 1.2f);
	m_pShaderManager->setVec4Value(g_ColorValueName, kGlassRGBA);
	m_basicMeshes->DrawCylinderMesh();

	// --- restore state ---
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
	// ================= END GLASS SHADE =================

	//** Bulb halo (glow) **//
//**************************//
	glDepthMask(GL_FALSE);                                          // no depth writes
	glDisable(GL_DEPTH_TEST);                                       // disable depth test so halo shows through glass
	glEnable(GL_BLEND);                                             // enable blending
	glBlendFunc(GL_ONE, GL_ONE);                                    // additive blend

	scaleXYZ = glm::vec3(0.32f, 0.44f, 0.32f);                   // halo size
	positionXYZ = glm::vec3(0.0f, 10.15f, -3.5f);                     // halo position
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	m_pShaderManager->setBoolValue("bUseLighting", false);          // emissive color
	SetShaderColor(0.22f, 0.19f, 0.08f, 1.0f);                      // warm glow
	m_basicMeshes->DrawSphereMesh();                                // draw halo

	m_pShaderManager->setBoolValue("bUseLighting", true);           // restore lighting
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);              // restore blend
	glEnable(GL_DEPTH_TEST);                                        // re-enable depth test
	glDepthMask(GL_TRUE);                                           // re-enable depth writes
}
