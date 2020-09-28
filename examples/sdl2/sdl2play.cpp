#include <stdio.h>
#include <iostream>
#include "sdk/player.h"
#include "VideoRenderer.h"
#include "sdk/AVLog.h"
#include "SDL.h"

using namespace FFPRO;

const Uint32 update_event = SDL_USEREVENT + 10;

int main(int argc, char *argv[])
{
    if (argc < 2) {
        AVDebug("Use 'Puff.exe file-name'\n");
        system("pause");
        return 0;
    }
    int w = 800, h = 480;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // (default) this enables vsync
    SDL_Window* window = SDL_CreateWindow("FFPRO", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    SDL_GL_SetSwapInterval(1);
	SDL_GL_SwapWindow(window);

    FFPRO::setLogFile("E:/log.txt");
    char* fileName = argv[1];
    AVDebug("The name of file to play: %s\n", fileName);

    Player *player = new Player();
	player->loadGLLoader(SDL_GL_GetProcAddress);
    player->setMedia(fileName);
    player->addVideoRenderer(w, h);
//    player->setWantedStreamSpec(MediaTypeAudio, "3");
    player->setMediaStatusCallback([window](FFPRO::MediaStatus status){
        switch (status) {
        case FFPRO::Loaded:
            SDL_SetWindowTitle(window, "LoadedMedia");
            break;
        case FFPRO::End:
            SDL_SetWindowTitle(window, "EndOfMedia");
            break;
        case FFPRO::Buffered:
            SDL_SetWindowTitle(window, "Buffered");
            break;
        }
    });
    player->setRenderCallback([](void*){
		SDL_Event e;
		e.type = update_event;
		SDL_PushEvent(&e);
    });
    player->setBufferProcessCallback([window](float p) {
        char title[64];
        sprintf(title, "Buffering: %d%%", FORCE_INT(p * 100));
        SDL_SetWindowTitle(window, title);
    });
    player->setBufferPara(BufferTime, 3 * 1000);
    //player->setClockType(SyncToVideo);
    player->prepare();

    /*Set window icon*/
    SDL_Surface *icon = SDL_LoadBMP("app.bmp");
    if (icon) {
        SDL_SetColorKey(icon, SDL_TRUE, SDL_MapRGB(icon->format, 255, 0, 255));
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    }
    SDL_Event event;
    while (1) {
        SDL_WaitEvent(&event);
        if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED
				|| event.window.event == SDL_WINDOWEVENT_EXPOSED) {
				SDL_GetWindowSize(window, &w, &h);
                player->resizeWindow(w, h);
			}
        }
        else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_SPACE) {
                player->pause(!player->isPaused());
            } else if (event.key.keysym.sym == SDLK_RIGHT) {
                player->seekForward();
            } else if (event.key.keysym.sym == SDLK_LEFT) {
                player->seekBackward();
            } else if (event.key.keysym.sym == SDLK_DOWN) {
                player->setSpeed(player->speed() - 0.1);
            } else if (event.key.keysym.sym == SDLK_UP) {
                player->setSpeed(player->speed() + 0.1);
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
            double x;
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button != SDL_BUTTON_RIGHT)
                    continue;
                x = event.button.x;
                int64_t ts; float frac;
                int ns, hh, mm, ss;
                int tns, thh, tmm, tss;
                tns = player->duration();
                thh = tns / 3600;
                tmm = (tns % 3600) / 60;
                tss = (tns % 60);
                frac = x / w;
                ns = frac * tns;
                hh = ns / 3600;
                mm = (ns % 3600) / 60;
                ss = (ns % 60);
                AVDebug("Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d)\n", frac * 100,
                    hh, mm, ss, thh, tmm, tss);
                ts = frac * player->duration();
                player->seek(ts);
            }
        }
		else if (event.type == update_event) {
            player->renderVideo();
            SDL_GL_SwapWindow(window);
			continue;;
        }
        else if (event.type == SDL_QUIT) {
            player->stop();
            break;
        }
    }
    delete player;
    SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
    AVDebug("player uninitialize\n");
    return 0;
}