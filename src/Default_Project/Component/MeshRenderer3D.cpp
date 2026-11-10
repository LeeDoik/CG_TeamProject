#include "MeshRenderer3D.h"
#include "Mesh.h"
#include "Material.h"
#include "Object.h"
#include "Transform.h"
#include "Camera.h"
#include "Light.h"
#include <gl/glew.h>
#include <gl/glm/gtc/type_ptr.hpp>
using namespace std;
using namespace glm;

extern map<string, Mesh*> meshChache;
extern map<string, Material*> materialChache;

MeshRenderer3D::MeshRenderer3D(Mesh* mesh, Material* material) : mesh(mesh) {
	this->materials.push_back(material);
}

void MeshRenderer3D::Render(Camera* activeCamera) {
	for (auto& material : materials) {
		//셰이더 사용
		material->Use();
		//VAO 바인딩
		mesh->Bind();

		//Transform의 worldMatrix를 셰이더에 전달
		Transform* transform = gameObject->GetComponent<Transform>();
		GLuint modelLoc = glGetUniformLocation(material->shaderProgram, "model");
		GLuint colorLoc = glGetUniformLocation(material->shaderProgram, "tColor");

		// View, Projection 행렬 전달
		if (activeCamera != nullptr) {
			GLuint viewLoc = glGetUniformLocation(material->shaderProgram, "view");
			glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(activeCamera->GetViewMatrix()));

			GLuint projLoc = glGetUniformLocation(material->shaderProgram, "proj");
			glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(activeCamera->GetProjectionMatrix()));

			//카메라의 월드 위치(viewPos) 전달
			Transform* camTr = activeCamera->gameObject->GetComponent<Transform>();
			GLuint viewPosLoc = glGetUniformLocation(material->shaderProgram, "viewPos");
			glUniform3fv(viewPosLoc, 1, value_ptr(camTr->worldPosition));
		}

		//1. 씬에 등록된 라이트 개수를 먼저 전달
		const auto& lights = gameObject->ctx.lights;
		int numLights = std::min(static_cast<int>(lights.size()), 10); // 셰이더 한계인 10개로 제한
		glUniform1i(glGetUniformLocation(material->shaderProgram, "numLights"), numLights);

		//2. 배열을 순회하며 각 광원 정보 전달
		for (int i = 0; i < numLights; ++i) {
			Light* sceneLight = lights[i];
			Transform* lightTr = sceneLight->gameObject->GetComponent<Transform>();
			if (lightTr == nullptr) continue;

			// GLSL 배열 인덱스 문자열 생성 (예: "lights[0].color")
			string baseName = "lights[" + to_string(i) + "].";

			// 공통 속성 전달 (타입, 색상, 방향)
			int typeInt = static_cast<int>(sceneLight->lightType);
			vec3 finalLightColor = vec3(sceneLight->lightColor) * sceneLight->intensity;
			vec3 lightDir = normalize(vec3(lightTr->worldMatrix * vec4(0.0f, 0.0f, -1.0f, 0.0f)));

			glUniform1i(glGetUniformLocation(material->shaderProgram, (baseName + "type").c_str()), typeInt);
			glUniform3fv(glGetUniformLocation(material->shaderProgram, (baseName + "color").c_str()), 1, value_ptr(finalLightColor));
			glUniform3fv(glGetUniformLocation(material->shaderProgram, (baseName + "direction").c_str()), 1, value_ptr(lightDir));

			// Point, Spot 속성 전달 (위치, 감쇠값)
			if (sceneLight->lightType == LightType::Spot || sceneLight->lightType == LightType::Point) {
				vec3 lightPos = vec3(lightTr->worldMatrix[3]); // Transform 4번째 열(위치값)
				glUniform3fv(glGetUniformLocation(material->shaderProgram, (baseName + "position").c_str()), 1, value_ptr(lightPos));

				glUniform1f(glGetUniformLocation(material->shaderProgram, (baseName + "constant").c_str()), sceneLight->constant);
				glUniform1f(glGetUniformLocation(material->shaderProgram, (baseName + "linear").c_str()), sceneLight->linear);
				glUniform1f(glGetUniformLocation(material->shaderProgram, (baseName + "quadratic").c_str()), sceneLight->quadratic);

				if (sceneLight->lightType == LightType::Spot) {
					glUniform1f(glGetUniformLocation(material->shaderProgram, (baseName + "cutOff").c_str()), glm::cos(glm::radians(sceneLight->cutOff)));
					glUniform1f(glGetUniformLocation(material->shaderProgram, (baseName + "outerCutOff").c_str()), glm::cos(glm::radians(sceneLight->outerCutOff)));
				}
			}
		}

		//Pass1 : 외곽선 렌더링 처리
		if (material->isOutline) {
			mat4 outlineMatrix = transform->worldMatrix;
			outlineMatrix = glm::scale(outlineMatrix, vec3{ 1.5f }); //외곽선 확대
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(outlineMatrix));
			glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f); //외곽선 색상 검정
			glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_BYTE, 0);

			//원래 색상 전달
			vec4 originalColor = material->GetVec4("tColor");
			glUniform4f(colorLoc, originalColor.r, originalColor.g, originalColor.b, originalColor.a);
		}

		//Pass2 : Mesh 데이터 그리기
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(transform->worldMatrix));
		if (isValid) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
		}
		if (isWireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		glBindVertexArray(0);
	}
}

Component* MeshRenderer3D::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	if (props.contains("mesh") == false || props.contains("materials") == false) {
		cout << obj->name << " 오브젝트의 MeshRenderer3D 컴포넌트에 mesh와 materials 속성이 필요합니다." << endl;
		return nullptr;
	}

	auto matData = props["materials"][0];
	Material* material = materialChache[matData];

	if (matData.contains("boolUniforms")) {
		for (auto& [key, val] : matData["boolUniforms"].items()) {
			material->SetBool(key, val);
		}
	}
	if (matData.contains("intUniforms")) {
		for (auto& [key, val] : matData["intUniforms"].items()) {
			material->SetInt(key, val);
		}
	}
	if (matData.contains("floatUniforms")) {
		for (auto& [key, val] : matData["floatUniforms"].items()) {
			material->SetFloat(key, val);
		}
	}
	if (matData.contains("vec2Uniforms")) {
		for (auto& [key, val] : matData["vec2Uniforms"].items()) {
			material->SetVec2(key, vec2(val[0], val[1]));
		}
	}
	if (matData.contains("vec3Uniforms")) {
		for (auto& [key, val] : matData["vec3Uniforms"].items()) {
			material->SetVec3(key, vec3(val[0], val[1], val[2]));
		}
	}
	if (matData.contains("vec4Uniforms")) {
		for (auto& [key, val] : matData["vec4Uniforms"].items()) {
			material->SetVec4(key, vec4(val[0], val[1], val[2], val[3]));
		}
	}

	MeshRenderer3D* meshRenderer = obj->AddComponent<MeshRenderer3D>(meshChache[props["mesh"]], material);

	if (props.contains("isWireframe")) meshRenderer->isWireframe = props["isWireframe"];

	return meshRenderer;
}

REGISTER_COMPONENT(MeshRenderer3D)