/*
Copyright (c) 2014 Aerys

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

#include "minko/file/FileProtocol.hpp"

#include "minko/file/Options.hpp"
#include "minko/Signal.hpp"
#include "minko/AbstractCanvas.hpp"
#include "minko/async/Worker.hpp"

#include <fstream>
#include <regex>

using namespace minko;
using namespace minko::file;

FileProtocol::FileProtocol()
{
}

std::list<std::shared_ptr<FileProtocol>>
FileProtocol::_runningLoaders;

void
FileProtocol::load()
{
    auto loader = std::static_pointer_cast<FileProtocol>(shared_from_this());

    _runningLoaders.push_back(loader);

    const auto& resolvedFilename = this->resolvedFilename();
    auto options = _options;
    auto flags = std::ios::in | std::ios::ate | std::ios::binary;

    std::string cleanFilename = "";

    for (uint i = 0; i < resolvedFilename.length(); ++i)
    {
        if (i < resolvedFilename.length() - 2 && resolvedFilename.at(i) == ':' && resolvedFilename.at(i + 1) == '/' && resolvedFilename.at(i + 2) == '/')
        {
            cleanFilename = "";
            i += 2;
            continue;
        }

        cleanFilename += resolvedFilename.at(i);
    }

    _options = options;

    auto realFilename = cleanFilename;

    std::fstream file(cleanFilename, flags);

    if (file.is_open())
    {
        if (_options->loadAsynchronously() && AbstractCanvas::defaultCanvas() != nullptr
            && AbstractCanvas::defaultCanvas()->isWorkerRegistered("file-protocol"))
        {
            file.close();
            auto worker = AbstractCanvas::defaultCanvas()->getWorker("file-protocol");

            _workerSlots.push_back(worker->message()->connect([=](async::Worker::Ptr, async::Worker::Message message)
            {
                if (message.type == "complete")
                {
                    void* bytes = &*message.data.begin();
                    data().assign(static_cast<unsigned char*>(bytes), static_cast<unsigned char*>(bytes) + message.data.size());
                    _complete->execute(loader);
                    _runningLoaders.remove(loader);
                }
                else if (message.type == "progress")
                {
                    float ratio = *reinterpret_cast<float*>(&*message.data.begin());

                    _progress->execute(loader, ratio);
                }
                else if (message.type == "error")
                {
                    _error->execute(loader);
                    _runningLoaders.remove(loader);
                }
            }));

            auto offset = options->seekingOffset();
            auto length = options->seekedLength();

            std::vector<char> offsetByteArray(4);
            offsetByteArray[0] = (offset & 0xff000000) >> 24;
            offsetByteArray[1] = (offset & 0x00ff0000) >> 16;
            offsetByteArray[2] = (offset & 0x0000ff00) >> 8;
            offsetByteArray[3] = (offset & 0x000000ff);

            std::vector<char> lengthByteArray(4);
            lengthByteArray[0] = (length & 0xff000000) >> 24;
            lengthByteArray[1] = (length & 0x00ff0000) >> 16;
            lengthByteArray[2] = (length & 0x0000ff00) >> 8;
            lengthByteArray[3] = (length & 0x000000ff);

            std::vector<char> input;
            
            input.insert(input.end(), offsetByteArray.begin(), offsetByteArray.end());
            input.insert(input.end(), lengthByteArray.begin(), lengthByteArray.end());
            input.insert(input.end(), cleanFilename.begin(), cleanFilename.end());

            worker->start(input);
        }
        else
        {
            auto offset = options->seekingOffset();

			auto length = options->seekedLength() > 0 ? options->seekedLength() : (unsigned int)file.tellg();

            // FIXME: use fixed size buffers and call _progress accordingly

            _progress->execute(shared_from_this(), 0.0);

			data().resize(length);

			file.seekg(offset, std::ios::beg);
			file.read((char*)&data()[0], length);
            file.close();

            _progress->execute(loader, 1.0);

            _complete->execute(shared_from_this());
            _runningLoaders.remove(loader);
        }
    }
    else
    {
        _error->execute(shared_from_this());
    }
}

bool
FileProtocol::fileExists(const std::string& filename)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary);

    return file.is_open();
}
