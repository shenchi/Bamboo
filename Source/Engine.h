#pragma once

namespace bamboo
{

	class Renderer;

	class Engine
	{
	public:

		int Init();

		int Run();

	private:

		Renderer*	renderer;
	};

}
