#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#define SDL_DISABLE_OLD_NAMES
#include <SDL3/SDL.h>
#include "SDL3_ttf/include/SDL3_ttf/SDL_ttf.h"

#define VM_SIZE 256

SDL_FRect get_letterbox(int width, int height) {
    if (width>height) {
        //space sides out
        float space = (width-height)/2;
        SDL_FRect r = {.x=space,.y=0,.w=height,.h=height};
        return r;
    } else {
        //space top and bottom out
        float space = (height-width)/2;
        SDL_FRect r = {.x=0,.y=space,.w=width,.h=width};
        return r;
    }
}
void update_texture(SDL_Texture *tex_vm, uint8_t *buffer,const SDL_Color *palette) {
    SDL_Color *pixels;
    int pitch;
    SDL_LockTexture(tex_vm,NULL,(void**)&pixels,&pitch);
    for (int i=0;i<VM_SIZE*VM_SIZE;i++) {
        pixels[i]=palette[buffer[i]];
    }
    SDL_UnlockTexture(tex_vm);
    
}
void update_texture_mem(SDL_Texture *tex_vm, uint8_t *mem,const SDL_Color *palette) {
    size_t i = ((int)mem[5])<<16;//0xXX0000
    update_texture(tex_vm,mem+i,palette);
}
void init_palette(SDL_Color *palette) {
    size_t i=0;
    for (int r=0;r<6;r++) {
        for (int b=0;b<6;b++) {
            for (int g=0;g<6;g++) {
                SDL_Color *pixel = &(palette[i]);
                pixel->r=r*0x33;
                pixel->g=b*0x33;
                pixel->b=g*0x33;
                pixel->a=SDL_ALPHA_OPAQUE;
                i+=1;
            }   
        }    
    }
    for (i=216;i<256;i++) {
        SDL_Color *pixel = &(palette[i]);
        pixel->r=0;
        pixel->g=0;
        pixel->b=0;
        pixel->a=SDL_ALPHA_OPAQUE;
        i+=1;
    }
}

uint32_t read_24BE_to_LE(uint8_t* mem, size_t addr) {
    uint8_t *pc_ptr = &mem[addr];
    //TODO remove the mask bc i think it's not necessary
    return (((uint32_t)pc_ptr[0])<<16 | ((uint32_t)pc_ptr[1])<<8 | ((uint32_t)pc_ptr[2]))&0x00ffffff;
}
uint32_t read_24BE(uint8_t* mem, size_t addr) {
    uint8_t *pc_ptr = &mem[addr];
    return (((uint32_t)pc_ptr[2])<<16 | ((uint32_t)pc_ptr[1])<<8 | ((uint32_t)pc_ptr[0]))&0x00ffffff;
}
void write_24LE_to_BE(uint8_t* mem, size_t addr,uint32_t LE_value) {
    uint8_t *pc_ptr = &mem[addr];
    mem[addr+0] = (LE_value>>16)&0xff;
    mem[addr+1] = (LE_value>>8)&0xff;
    mem[addr+2] = (LE_value>>0)&0xff;
}
void write_24BE(uint8_t* mem, size_t addr,uint32_t BE_value) {
    uint8_t *pc_ptr = &mem[addr];
    mem[addr+2] = (BE_value>>16)&0xff;
    mem[addr+1] = (BE_value>>8)&0xff;
    mem[addr+0] = (BE_value>>0)&0xff;
}
size_t file_len(FILE* f) {
    fseek(f,0L,SEEK_END);
    size_t len = ftell(f);
    fseek(f,0L,SEEK_SET);
    return len;
}

SDL_Window *w=NULL;
SDL_Texture *tex_vm=NULL;
void cb_sdl(void) {
    if (tex_vm)
        SDL_DestroyTexture(tex_vm);
    if (w)
        SDL_DestroyWindow(w);
    SDL_Quit();
}
TTF_TextEngine *text_engine=NULL;
TTF_Font *FONT=NULL;
TTF_Text *MESSAGE_DROP_TTF=NULL;TTF_Text *MESSAGE_BIG_TTF=NULL;
void cb_text_engine(void) {
    if (MESSAGE_DROP_TTF)
        TTF_DestroyText(MESSAGE_DROP_TTF);
    if (MESSAGE_BIG_TTF)
        TTF_DestroyText(MESSAGE_BIG_TTF);
    if (FONT) 
        TTF_CloseFont(FONT);
    if (text_engine)
        TTF_DestroyRendererTextEngine(text_engine);
    TTF_Quit();
}

#define RAM_SIZE 0x1000000
uint8_t *mem=NULL;
void cb_free_mem(void) {
    free(mem);
}
int main(int argc, char const *argv[]) {
    //data
    SDL_Color palette[256];
    
    uint8_t *mem = calloc(1,RAM_SIZE);
    atexit(cb_free_mem);
    init_palette(palette);
    bool playing = false;
    const char* MESSAGE_DROP = "Drop a file onto this window";
    const char* MESSAGE_BIG = "File too large!";
    const char* message = MESSAGE_DROP;
    //parse args for file
    if (argc>1) {
        const char* fn = argv[1];
        printf("opening file: %s\n",fn);
        FILE *f = fopen(fn,"r");
        if (!f) {
            perror("bytepusher");
            return EXIT_FAILURE;
        }
        
        size_t len = file_len(f);
        if (len>RAM_SIZE) {
            message=MESSAGE_BIG;
        } else {
            //load the file
            fread(mem,1,len,f);
            playing=true;
        }
        fclose(f);
    }

    
    const SDL_Keycode key_map[16] = {
        SDLK_X,SDLK_1,SDLK_2,SDLK_3,
        SDLK_Q,SDLK_W,SDLK_E,SDLK_A,
        SDLK_A,SDLK_S,SDLK_D,SDLK_Z,
        SDLK_C,SDLK_R,SDLK_F,SDLK_C,
    };



    //SDL Stuff
    bool result;
    result = SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_EVENTS);
    if (!result) {
        fprintf(stderr,"SDL Error: %s\n",SDL_GetError());
        return EXIT_FAILURE;
    }
    atexit(cb_sdl);
    
    SDL_Renderer *r;
    result = SDL_CreateWindowAndRenderer("BytePusher Emulator :D",VM_SIZE,VM_SIZE,SDL_WINDOW_RESIZABLE,&w,&r);
    if (!result) {
        fprintf(stderr,"SDL Error: %s\n",SDL_GetError());
        return EXIT_FAILURE;
    }
    SDL_SetRenderVSync(r,true);
    if (!TTF_Init()) {
        fprintf(stderr,"SDL_ttf Error: %s\n",SDL_GetError());
        return EXIT_FAILURE;
    }
    atexit(cb_text_engine);
    text_engine = TTF_CreateRendererTextEngine(r);
    if (!text_engine) {
        fprintf(stderr,"SDL_ttf Error: %s\n",SDL_GetError());
        return EXIT_FAILURE;
    }
    FONT=TTF_OpenFont("NotoSans-Regular.ttf",24.0);
    if (!FONT) {
        fprintf(stderr,"SDL_ttf Error: %s\n",SDL_GetError());
        return EXIT_FAILURE;
    }
    MESSAGE_DROP_TTF = TTF_CreateText(text_engine,FONT,MESSAGE_DROP,strlen(MESSAGE_DROP));
    MESSAGE_BIG_TTF = TTF_CreateText(text_engine,FONT,MESSAGE_BIG,strlen(MESSAGE_BIG));
    
    tex_vm = SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,256,256);
    if (!tex_vm) {
        fprintf(stderr,"SDL Error: %s\n",SDL_GetError());
        return EXIT_FAILURE;
    }
    SDL_SetTextureScaleMode(tex_vm,SDL_SCALEMODE_NEAREST);
    update_texture_mem(tex_vm,mem,palette);

    
    bool should_exit = false;
    SDL_Event e;
    SDL_Time ticks=0,old_ticks;
    SDL_GetCurrentTime(&ticks);
    SDL_Time diff,diff_real;
    while (!should_exit) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT: {
                    should_exit = true;
                    break;      
                }
                case SDL_EVENT_KEY_DOWN: {
                    if (e.key.key==SDLK_ESCAPE) {
                        should_exit=true;
                        break;
                    }
                    break;
                }
                case SDL_EVENT_DROP_FILE: {
                    const char *fn = e.drop.data;
                    printf("opening file: %s\n",fn);
                    FILE *f = fopen(fn,"r");
                    bool error = false;
                    if (!f) {
                        message=strerror(errno);
                        error=true;
                    } else {   
                        size_t len = file_len(f);
                        if (len>0x10000) {
                            message=MESSAGE_BIG;
                        } else {
                            //load the file
                            fread(mem,len,1,f);
                            playing=true;
                        }
                        fclose(f);
                    }
                }
            }
            if (e.type==SDL_EVENT_KEY_DOWN||e.type==SDL_EVENT_KEY_UP) {
                int i=0;
                while (key_map[i]!=e.key.key) {
                    i++;
                }
                if (key_map[i]==e.key.key) {
                    if (i>7) {
                        if (e.key.down) {
                            mem[0] |= 1<<(i-8);
                        } else {
                            mem[0] &= ~(1<<(i-8));
                        }
                    } else {
                        if (e.key.down) {
                            mem[1] |= 1<<i;
                        } else {
                            mem[1] &= ~(1<<i);
                        }
                    }
                }
            }
            if (should_exit) break;
        }
        if (should_exit) break;

        //update
        int width,height;
        SDL_GetRenderOutputSize(r,&width,&height);
        SDL_FRect letterbox_rect = get_letterbox(width,height);

        if (playing) {
            size_t pc=read_24BE_to_LE(mem,2);
            for (int i=0;i<65536;i++) {
                //24 bits
                // Copy 1 byte from A to B, then jump to C. 
                uint8_t *pc_ptr = &mem[pc];
                size_t a = read_24BE_to_LE(mem,pc+0);
                size_t b = read_24BE_to_LE(mem,pc+3);
                size_t c = read_24BE_to_LE(mem,pc+6);
                // An instruction should be able to modify its own jump address before jumping. 
                // Thus the copy operation must be completed before the jump address is read. 
                mem[b]=mem[a];
                pc=c;
                // printf("\x1b[H%02X%02X %03lX %03lX %03lX %03lX\n",mem[2],mem[3],a,b,c,pc);
                // SDL_Delay(500);
            }
            update_texture_mem(tex_vm,mem,palette);
            //TODO audio
        }


        //draw
        SDL_SetRenderDrawColor(r,0,0,0,SDL_ALPHA_OPAQUE);
        SDL_RenderClear(r);

        SDL_RenderTexture(r,tex_vm,NULL,&letterbox_rect);
        //get fps
        // double fps = (double)1000000000.0/(double)diff_real;
        // char buf[10] = {0};
        // int len = snprintf(buf,10,"%lf",fps);
        // TTF_Text *fps_text = TTF_CreateText(text_engine,FONT,buf,len);
        // TTF_DrawRendererText(fps_text,100.,100.);
        // TTF_DestroyText(fps_text);
        if (!playing) {
            TTF_Text* current_ttf_text;
            if (message==MESSAGE_BIG) {
                current_ttf_text=MESSAGE_BIG_TTF;
            } else if (message==MESSAGE_DROP) {
                current_ttf_text=MESSAGE_DROP_TTF;
            }
            int text_w,text_h;
            TTF_GetStringSize(FONT,message,strlen(message),&text_w,&text_h);
            TTF_DrawRendererText(current_ttf_text,(width-text_w)/2,height/2);
        }
        
        SDL_RenderPresent(r);
        // (0.0166666666667 * 1000) * 1000 * 1000
        //16666666 60fps in ns
        // 1000000000 / 61
        //16393442 for 61fps
        //16388 60fps-1 in ns
        SDL_Time now=0;
        SDL_GetCurrentTime(&now);
        diff = now-ticks;
        const SDL_Time wait = 16393442;
        if (diff<wait) {
            SDL_DelayNS(wait-diff);     
            // printf("%lu\n",wait-diff);
        } 
        SDL_GetCurrentTime(&ticks);
        diff_real = ticks-old_ticks;
        SDL_GetCurrentTime(&old_ticks);
    }
    return EXIT_SUCCESS;
}
