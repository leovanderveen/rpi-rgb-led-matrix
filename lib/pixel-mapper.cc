// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2018 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

#include "pixel-mapper.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>

namespace rgb_matrix {
    namespace {
        class RotatePixelMapper : public PixelMapper {
        public:
            RotatePixelMapper() : angle_(0) {}
            
            virtual const char *GetName() const { return "Rotate"; }
            
            virtual bool SetParameters(int chain, int parallel, const char *param) {
                if (param == NULL || strlen(param) == 0) {
                    angle_ = 0;
                    return true;
                }
                char *errpos;
                fprintf(stderr,"pixelmapper v2.0 by nr37\nUse: Rotate:37;\n(~/rpi-rgb-led-matrix/lib/pixel-mapper.cc)\n");
                const int angle = strtol(param, &errpos, 10);
                angle_ = (angle + 360) % 360;
                return true;
            }
            
            virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                                        int *visible_width, int *visible_height)
            const {
                
                if(angle_ == 37) {
                    fprintf(stderr,"pixelmapper: 37\n");
                    *visible_width = 96;
                    *visible_height = 64;
                }
                
                if (angle_ == 38) {
                    fprintf(stderr,"pixelmapper: 38\n");
                    *visible_width = 96;
                    *visible_height = 64;
                }
                return true;
            }
            
            virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                            int x, int y,
                                            int *matrix_x, int *matrix_y) const {
                
                if(angle_ == 37) {
                    if(x >= 32) {
                        if(y < 32) {
                            // x from 32-95
                            // y from 0-31
                            *matrix_x = 95-x; // matrix_x from 0-63
                            *matrix_y = 31-y;
                        } else {
                            // x from 32-95
                            // y from 32-63
                            *matrix_x = 96+x; // matrix_x from 128-191
                            *matrix_y = y-32;
                        }
                    } else {
                        // x from 0-31
                        // y from 0-63
                        *matrix_x = 64+y; // matrix_x from 64-127
                        *matrix_y = 31-x;
                    }
                }
                
                if(angle_ == 38){
                    if(x>=0 && x<32) {
                        // x from 0-31
                        // y from 0-63
                        *matrix_x = 128+y; // 128-191
                        *matrix_y = x; // 0-31
                    }
                    if(x>=32 && x<64) {
                        // x from 32-63
                        // y from 0-63
                        *matrix_x = 64+y; // 64-127
                        *matrix_y = x-32; // 0-31
                    }
                    if(x>=64&&x<96){
                        // x from 64-95
                        // y from 0-63
                        *matrix_x = y; // 0-63
                        *matrix_y = x-64; //0-31
                    }
                }
            }
            
        private:
            int angle_;
        };
        
        // If we take a long chain of panels and arrange them in a U-shape, so
        // that after half the panels we bend around and continue below. This way
        // we have a panel that has double the height but only uses one chain.
        // A single chain display with four 32x32 panels can then be arranged in this
        // 64x64 display:
        //    [<][<][<][<] }- Raspbery Pi connector
        //
        // can be arranged in this U-shape
        //    [<][<] }----- Raspberry Pi connector
        //    [>][>]
        //
        // This works for more than one chain as well. Here an arrangement with
        // two chains with 8 panels each
        //   [<][<][<][<]  }-- Pi connector #1
        //   [>][>][>][>]
        //   [<][<][<][<]  }--- Pi connector #2
        //   [>][>][>][>]
        class UArrangementMapper : public PixelMapper {
        public:
            UArrangementMapper() : parallel_(1) {}
            
            virtual const char *GetName() const { return "U-mapper"; }
            
            virtual bool SetParameters(int chain, int parallel, const char *param) {
                if (chain < 2) {  // technically, a chain of 2 would work, but somewhat pointless
                    fprintf(stderr, "U-mapper: need at least --led-chain=4 for useful folding\n");
                    return false;
                }
                if (chain % 2 != 0) {
                    fprintf(stderr, "U-mapper: Chain (--led-chain) needs to be divisible by two\n");
                    return false;
                }
                parallel_ = parallel;
                return true;
            }
            
            virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                                        int *visible_width, int *visible_height)
            const {
                *visible_width = (matrix_width / 64) * 32;   // Div at 32px boundary
                *visible_height = 2 * matrix_height;
                if (matrix_height % parallel_ != 0) {
                    fprintf(stderr, "%s For parallel=%d we would expect the height=%d "
                            "to be divisible by %d ??\n",
                            GetName(), parallel_, matrix_height, parallel_);
                    return false;
                }
                return true;
            }
            
            virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                            int x, int y,
                                            int *matrix_x, int *matrix_y) const {
                const int panel_height = matrix_height / parallel_;
                const int visible_width = (matrix_width / 64) * 32;
                const int slab_height = 2 * panel_height;   // one folded u-shape
                const int base_y = (y / slab_height) * panel_height;
                y %= slab_height;
                if (y < panel_height) {
                    x += matrix_width / 2;
                } else {
                    x = visible_width - x - 1;
                    y = slab_height - y - 1;
                }
                *matrix_x = x;
                *matrix_y = base_y + y;
            }
            
        private:
            int parallel_;
        };
        
        typedef std::map<std::string, PixelMapper*> MapperByName;
        static void RegisterPixelMapperInternal(MapperByName *registry,
                                                PixelMapper *mapper) {
            assert(mapper != NULL);
            std::string lower_name;
            for (const char *n = mapper->GetName(); *n; n++)
                lower_name.append(1, tolower(*n));
            (*registry)[lower_name] = mapper;
        }
        
        static MapperByName *CreateMapperMap() {
            MapperByName *result = new MapperByName();
            
            // Register all the default PixelMappers here.
            RegisterPixelMapperInternal(result, new RotatePixelMapper());
            RegisterPixelMapperInternal(result, new UArrangementMapper());
            return result;
        }
        
        static MapperByName *GetMapperMap() {
            static MapperByName *singleton_instance = CreateMapperMap();
            return singleton_instance;
        }
    }  // anonymous namespace
    
    // Public API.
    void RegisterPixelMapper(PixelMapper *mapper) {
        RegisterPixelMapperInternal(GetMapperMap(), mapper);
    }
    
    std::vector<std::string> GetAvailablePixelMappers() {
        std::vector<std::string> result;
        MapperByName *m = GetMapperMap();
        for (MapperByName::const_iterator it = m->begin(); it != m->end(); ++it) {
            result.push_back(it->second->GetName());
        }
        return result;
    }
    
    const PixelMapper *FindPixelMapper(const char *name,
                                       int chain, int parallel,
                                       const char *parameter) {
        std::string lower_name;
        for (const char *n = name; *n; n++) lower_name.append(1, tolower(*n));
        MapperByName::const_iterator found = GetMapperMap()->find(lower_name);
        if (found == GetMapperMap()->end()) {
            fprintf(stderr, "%s: no such mapper\n", name);
            return NULL;
        }
        PixelMapper *mapper = found->second;
        if (mapper == NULL) return NULL;  // should not happen.
        if (!mapper->SetParameters(chain, parallel, parameter))
            return NULL;   // Got parameter, but couldn't deal with it.
        return mapper;
    }
}  // namespace rgb_matrix
