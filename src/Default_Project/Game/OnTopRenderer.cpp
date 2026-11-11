#include "OnTopRenderer.h"
#include <gl/glew.h>

bool OnTopRenderer::drawOnTop = false;

void OnTopRenderer::Render(Camera* activeCamera) {
	if (drawOnTop) glDepthRange(0.0, 0.02);
	MeshRenderer3D::Render(activeCamera);
	if (drawOnTop) glDepthRange(0.0, 1.0);
}
