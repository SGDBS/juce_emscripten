/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2013 - Raw Material Software Ltd.

   Permission is granted to use this software under the terms of either:
   a) the GPL v2 (or any later version)
   b) the Affero GPL v3

   Details of these licenses can be found at: www.gnu.org/licenses

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.juce.com for more information.

  ==============================================================================
*/

#include <emscripten.h>

#include <deque>
#include <mutex>

namespace juce
{

static void createDirIfNotExists(File::SpecialLocationType type)
{
    File dir = File::getSpecialLocation(type);
    if (! dir.exists()) dir.createDirectory();
}

void MessageManager::doPlatformSpecificInitialisation()
{
    createDirIfNotExists(File::userHomeDirectory);
    createDirIfNotExists(File::userDocumentsDirectory);
    createDirIfNotExists(File::userMusicDirectory);
    createDirIfNotExists(File::userMoviesDirectory);
    createDirIfNotExists(File::userPicturesDirectory);
    createDirIfNotExists(File::userDesktopDirectory);
    createDirIfNotExists(File::userApplicationDataDirectory);
    createDirIfNotExists(File::commonDocumentsDirectory);
    createDirIfNotExists(File::commonApplicationDataDirectory);
    createDirIfNotExists(File::globalApplicationsDirectory);
    createDirIfNotExists(File::tempDirectory);
}

void MessageManager::doPlatformSpecificShutdown() {}

//==============================================================================
bool MessageManager::dispatchNextMessageOnSystemQueue (const bool returnIfNoPendingMessages)
{
    Logger::outputDebugString ("*** Modal loops are not possible in Android!! Exiting...");
    exit (1);

    return true;
}

std::deque<MessageManager::MessageBase*> messageQueue;
std::mutex queueMtx;
std::atomic<bool> quitPosted{false};

std::vector<std::function<void()>> preDispatchLoopFuncs;
std::vector<std::function<void()>> postDispatchLoopFuncs;

void dispatchLoop()
{
    for (auto f : preDispatchLoopFuncs) f();

    queueMtx.lock();
    std::deque<MessageManager::MessageBase*> messageCopy = messageQueue;
    messageQueue.clear();
    queueMtx.unlock();
    
    while (! messageCopy.empty())
    {
        MessageManager::MessageBase* message = messageCopy.front();
        messageCopy.pop_front();
        // std::cout << "dispatchLoop-msg: " << message <<
        //             "type: " << typeid(*message).name() << std::endl;
        message->messageCallback();
        message->decReferenceCount();
    }
    
   #if DEBUG
    EM_ASM({
        var logArea = document.querySelector("#output");
        var n = logArea.value.length;
        if (n > 1000)
            logArea.value = logArea.value.substring(n - 1000, n);
    });
   #endif

    for (auto f : postDispatchLoopFuncs) f();

    if (quitPosted)
    {
        emscripten_cancel_main_loop();
    }
}

bool MessageManager::postMessageToSystemQueue (MessageManager::MessageBase* const message)
{
    queueMtx.lock();
    messageQueue.push_back(message);
    message -> incReferenceCount();
    queueMtx.unlock();
    return true;
}

void MessageManager::broadcastMessage (const String&)
{
}

void MessageManager::runDispatchLoop()
{
    emscripten_set_main_loop(dispatchLoop, 0, 0);
}

void MessageManager::stopDispatchLoop()
{
    struct QuitCallback  : public CallbackMessage
    {
        QuitCallback() {}

        void messageCallback() override { }
    };

    (new QuitCallback())->post();
    quitMessagePosted = true;
    quitPosted = true;
}

}
