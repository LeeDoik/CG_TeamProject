#version 330 core
// [Game] Paper shader = Standard_fragment.glsl lighting (teammate) + procedural paper texture.
in vec3 out_Color;
in vec3 FragPos;
in vec3 Normal;

uniform vec4 tColor;
uniform int meadowSurface;

float meadowHash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float meadowNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(meadowHash(i), meadowHash(i + vec2(1, 0)), f.x),
               mix(meadowHash(i + vec2(0, 1)), meadowHash(i + vec2(1, 1)), f.x), f.y);
}
vec3 meadowColor(vec3 base, vec2 p) {
    // Broad paper patches, a narrow worn trail, and tiny printed grass marks.
    float patch = meadowNoise(p * 0.48);
    base *= mix(vec3(0.83, 0.94, 0.81), vec3(1.12, 1.06, 0.94), patch);
    float center = 0.18 * sin(p.x * 0.42) + 0.08 * sin(p.x * 1.3);
    float edge = abs(p.y - center) + (meadowNoise(p * 3.2) - 0.5) * 0.14;
    float aa = max(fwidth(edge), 0.012);
    float trail = 1.0 - smoothstep(0.40 - aa, 0.55 + aa, edge);
    vec3 pathColor = mix(vec3(0.75, 0.69, 0.46), vec3(0.86, 0.80, 0.57), patch);
    base = mix(base, pathColor, trail * 0.83);
    vec2 cell = floor(p * 2.0), q = fract(p * 2.0) - 0.5;
    float seed = meadowHash(cell);
    q -= vec2(meadowHash(cell + 9.1), meadowHash(cell + 4.7)) * 0.28 - 0.14;
    float blade = min(abs(q.x - q.y * 0.38), abs(q.x + q.y * 0.5));
    float lineAA = max(fwidth(blade), 0.014);
    float mark = (1.0 - smoothstep(0.012, 0.012 + lineAA, blade)) *
                 (1.0 - smoothstep(0.06, 0.16, abs(q.y))) * step(0.28, seed);
    base = mix(base, base * vec3(0.68, 0.80, 0.60), mark * (1.0 - trail) * 0.60);
    float dotDistance = length(q - vec2(0.10, 0.08));
    float flower = (1.0 - smoothstep(0.022, 0.022 + max(fwidth(dotDistance), 0.012), dotDistance)) * step(0.86, seed);
    return mix(base, vec3(0.95, 0.88, 0.61), flower * (1.0 - trail));
}

// [Game] paper texture: world-space planar UV, so boxes of any size tile evenly
uniform sampler2D paperTex;
uniform float texScale;				
uniform vec3 viewPos;		

//다중 광원을 받기 위한 구조체 정의
struct Light {
    int type; // 0: Directional, 1: Point, 2: Spot
    vec3 position;
    vec3 direction;
    vec3 color;

    float constant;
    float linear;
    float quadratic;
    
    float cutOff;
    float outerCutOff;
};

#define MAX_LIGHTS 10
uniform Light lights[MAX_LIGHTS]; // 빛 정보 배열
uniform int numLights;            // 현재 씬에 존재하는 활성화된 빛의 개수

out vec4 FragColor;

void main()
{
	vec3 nTex = length(Normal) > 0.0 ? abs(normalize(Normal)) : vec3(0.0, 1.0, 0.0);
	vec2 uv = (nTex.x > nTex.y && nTex.x > nTex.z) ? FragPos.zy : ((nTex.y > nTex.z) ? FragPos.xz : FragPos.xy);
	vec3 paper = texture(paperTex, uv * texScale).rgb;

	vec3 norm = length(Normal) > 0.0 ? normalize(Normal) : vec3(0.0, 1.0, 0.0);
	vec3 viewDir = normalize(viewPos - FragPos);
	
	vec3 finalLighting = vec3(0.0); // 누적할 최종 조명 색상

	// 씬에 존재하는 모든 빛을 순회하며 조명 값 합산
	int count = min(numLights, MAX_LIGHTS);
	for(int i = 0; i < count; i++) {
		
		// 1. Ambient (환경광)
		float ambientStrength = 0.2f;
		vec3 ambient = ambientStrength * lights[i].color;

		vec3 lightDirNormalized;
		float attenuation = 1.0;
		float spotIntensity = 1.0;

		if (lights[i].type == 0) {
			// Directional Light
			lightDirNormalized = normalize(-lights[i].direction);
		} else {
			vec3 lightToFrag = FragPos - lights[i].position;
			float distance = length(lightToFrag);
			lightDirNormalized = normalize(lightToFrag);

			// 거리 감쇠 (Attenuation)
			attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));

			// 스팟 원뿔 각도 (Spotlight CutOff)
			if (lights[i].type == 2) {
				// 💡 핵심: 스팟 라이트일 때만 direction 벡터를 반대로(-lights[i].direction) 뒤집어 내적합니다.
				float theta = dot(lightDirNormalized, normalize(-lights[i].direction));
				float epsilon = lights[i].cutOff - lights[i].outerCutOff;
				spotIntensity = clamp((theta - lights[i].outerCutOff) / epsilon, 0.0, 1.0);
			}
		}

		// 2. Diffuse (난반사광)
		float diff = max(dot(norm, -lightDirNormalized), 0.0f);
		vec3 diffuse = diff * lights[i].color;

		// 3. Specular (정반사광)
		float shiness = 128.0;
		vec3 reflectDir = reflect(lightDirNormalized, norm);  
		float spec = pow(max(dot(viewDir, reflectDir), 0.0f), shiness);
		vec3 specular = spec * lights[i].color;

		diffuse *= attenuation * spotIntensity;
		specular *= attenuation * spotIntensity;

		// 누적
		finalLighting += (ambient + diffuse + specular);
	}

	vec3 surfaceColor = vec3(tColor) * paper;
	if (meadowSurface == 1 && norm.y > 0.5) surfaceColor = meadowColor(surfaceColor, FragPos.xz);
	vec3 result = finalLighting * surfaceColor;
	FragColor = vec4(result, tColor.a);
}