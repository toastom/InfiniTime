#pragma once

#include <lvgl/lvgl.h>
#include "displayapp/screens/Screen.h"
#include <vector>
#include <FreeRTOS.h>

namespace Pinetime{
    namespace Components{
        class LittleVgl;
    }
    namespace Applications{
        enum class WORM_DIR{UP, RIGHT, DOWN, LEFT};

        struct WormFood{
            int16_t row;
            int16_t col;
        };

        struct WormSegment{
            int16_t x;
            int16_t y;
            int16_t redirectX;
            int16_t redirectY;
            WORM_DIR currentDir;
        };

        namespace Screens{
            class Worm : public Screen{
                public:
                    Worm(DisplayApp *app, Pinetime::Components::LittleVgl& lvgl);
                    ~Worm() override;

                    void Refresh() override;
                    bool OnTouchEvent(TouchEvents event) override;
                private:
                    Pinetime::Components::LittleVgl& lvgl;

                    std::vector<struct WormSegment> worm;
                    struct WormFood food;
                    int scoreValue;
                    int8_t dx;
                    int8_t dy;

                    lv_obj_t *score;
                    lv_obj_t *grid;
                    lv_style_t bgStyle;
                    lv_style_t wormStyle;
                    lv_style_t foodStyle;
                    lv_task_t *taskRefresh;
                    TickType_t startTime;

                    void placeFood();
                    void growWorm(int16_t xOffset, int16_t yOffset);
                    void moveWorm();
                    void restartGame();

            };
        }
    }
}
