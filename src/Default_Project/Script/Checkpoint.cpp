#include "Checkpoint.h"
#include "Object.h"
#include "MeshRenderer3D.h"

// main.cpp에서 만든 재질 캐시 (LevelLoader.cpp와 같은 방식)
extern map<string, Material*> materialChache;

// 깃발의 재질(색)만 바꾼다. 색은 main.cpp에 이미 있는 임시 색을 쓴다.
void Checkpoint::SetActive(bool on) {
	MeshRenderer3D* mr = gameObject->GetComponent<MeshRenderer3D>();
	if (mr) mr->materials[0] = materialChache[on ? "DarkYellow" : "Green"];
}
