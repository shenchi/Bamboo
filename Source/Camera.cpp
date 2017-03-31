#include "Camera.h"

using namespace DirectX;

Camera::Camera()
	:
	position(0.0f, 0.0f, 0.0f),
	direction(0.0f, 0.0f, 1.0f),
	yaw(0.0f),
	pitch(0.0f),
	viewMatrix(),
	viewProjection(),
	dirty(false)
{
	XMStoreFloat4x4(&viewMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&viewProjection, XMMatrixIdentity());
}

void Camera::SetPerspective(float aspect, float fov, float zNear, float zFar)
{
	XMStoreFloat4x4(
		&viewProjection,
		XMMatrixTranspose(XMMatrixPerspectiveFovLH(fov, aspect, zNear, zFar))
	);
}

void Camera::SetPosition(const XMFLOAT3 & pos)
{
	position = pos;
	dirty = true;
}

void Camera::MoveAlongWorldAxes(float x, float y, float z)
{
	position.x += x;
	position.y += y;
	position.z += z;
	dirty = true;
}

void Camera::MoveAlongDirection(float forward, float right, float up)
{
	XMVECTOR vecF = XMLoadFloat3(&direction);
	XMVECTOR vecR = XMVector3Normalize(XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), vecF));
	XMVECTOR vecU = XMVector3Cross(vecF, vecR);
	XMVECTOR pos = XMLoadFloat3(&position);

	XMStoreFloat3(
		&position,
		pos + vecF * forward + vecR * right + vecU * up
	);
	
	dirty = true;
}

void Camera::SetRotation(float yaw, float pitch)
{
	this->yaw = yaw;
	this->pitch = pitch;

	XMStoreFloat3(
		&direction,
		XMVector3Rotate(
			XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			XMQuaternionRotationRollPitchYaw(pitch, yaw, 0.0f)
		)
	);

	dirty = true;
}

const XMFLOAT4X4 & Camera::GetViewMatrix()
{
	if (dirty)
	{
		dirty = false;
		XMStoreFloat4x4(
			&viewMatrix,
			XMMatrixTranspose(
				XMMatrixLookToLH(
					XMLoadFloat3(&position),
					XMLoadFloat3(&direction),
					XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
				)
			)
		);
	}

	return viewMatrix;
}

