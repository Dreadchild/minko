/*
Copyright (c) 2013 Aerys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include "minko/Common.hpp"

#include "minko/Signal.hpp"
#include "minko/render/Pass.hpp"

namespace minko
{
	namespace render
	{
		class Effect :
			public std::enable_shared_from_this<Effect>
		{
		public:
			typedef std::shared_ptr<Effect>	Ptr;

		private:
			typedef std::shared_ptr<Pass>										PassPtr;
			typedef std::vector<PassPtr> 										Technique;
			typedef Signal<Ptr, const std::string&, const std::string&>::Ptr	TechniqueChangedSignalPtr;

		private:
			std::unordered_map<std::string, Technique> 			_techniques;
			std::string											_currentTechniqueName;
			Technique 											_passes;

			std::list<std::function<void(PassPtr)>>				_uniformFunctions;

			TechniqueChangedSignalPtr							_techniqueChanged;

		public:
			inline static
			Ptr
			create()
			{
				return std::shared_ptr<Effect>(new Effect());
			}

			inline static
			Ptr
			create(std::vector<PassPtr>& passes)
			{
				auto effect = create();

				effect->_passes = effect->_techniques["default"] = passes;

				return effect;
			}

			inline
			const std::vector<PassPtr>&
			passes()
			{
				return _passes;
			}

            template <typename... T>
			void
			setUniform(const std::string& name, const T&... values)
			{
				_uniformFunctions.push_back(std::bind(
					&Effect::setUniformOnPass<T...>, shared_from_this(), std::placeholders::_1, name, values...
				));

				for (auto technique : _techniques)
					for (auto& pass : technique.second)
						pass->setUniform(name, values...);
			}

			inline
			const std::string&
			technique()
			{
				return _currentTechniqueName;
			}

			inline
			void
			technique(const std::string& technique)
			{
				if (_techniques.count(technique) == 0)
					throw std::invalid_argument("technique = " + technique);

				_currentTechniqueName = technique;
				_passes = _techniques[technique];
			}

			inline
			Signal<Ptr, const std::string&, const std::string&>::Ptr
			techniqueChanged() const
			{
				return _techniqueChanged;
			}

            void
            addTechnique(const std::string& name, Technique& passes);

            void
            removeTechnique(const std::string& name);

		private:
			Effect();

			template <typename... T>
			void
			setUniformOnPass(std::shared_ptr<Pass> pass, const std::string& name, const T&... values)
			{
				pass->setUniform(name, values...);
			}
		};		
	}
}
