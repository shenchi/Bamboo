#pragma once

#include <DirectXMath.h>

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4X4;

class Camera
{
public:
	Camera();
	
	void SetPerspective(float aspect, float fov, float zNear, float zFar);
	
	void SetPosition(const XMFLOAT3& pos);
	void SetPosition(float x, float y, float z) { SetPosition(XMFLOAT3(x, y, z)); }

	void MoveAlongWorldAxes(float x, float y, float z);
	void MoveAlongDirection(float forward, float right, float up);

	void SetRotation(float yaw, float pitch);

	const XMFLOAT3& GetPosition() const { return position; }
	const XMFLOAT3& GetDirection() const { return direction; }
	float GetYaw() const { return yaw; }
	float GetPitch() const { return pitch; }

	const XMFLOAT4X4& GetViewMatrix();
	const XMFLOAT4X4& GetProjectionMatrix() const { return viewProjection; }

private:

	XMFLOAT3		position;
	XMFLOAT3		direction;
	float			yaw;
	float			pitch;

	XMFLOAT4X4		viewMatrix;
	XMFLOAT4X4		viewProjection;

	bool			dirty;
};
