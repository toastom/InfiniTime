#include "displayapp/screens/Worm.h"
#include "displayapp/DisplayApp.h"
#include "displayapp/LittleVgl.h"

#include <iostream>
#include <cstdlib>
#include <FreeRTOS.h>
#include <task.h>

using namespace Pinetime::Applications::Screens;

const int WORM_SPEED = 0;
const int SEG_SIZE = 12; //px
const int FOOD_POINTS = 50;
const int GRID_COL_MAX = 10;
const int GRID_ROW_MAX = 9;

Worm::Worm(Pinetime::Applications::DisplayApp *app, Pinetime::Components::LittleVgl& lvgl) : Screen(app), lvgl{lvgl}
{
	// Background style
	lv_style_init(&bgStyle);
	lv_style_set_border_color(&bgStyle, LV_STATE_DEFAULT, LV_COLOR_WHITE);
	lv_style_set_bg_opa(&bgStyle, LV_STATE_DEFAULT, LV_OPA_COVER);
	lv_style_set_bg_color(&bgStyle, LV_STATE_DEFAULT, LV_COLOR_BLACK);

	// Worm segment style
	lv_style_init(&wormStyle);
	lv_style_set_border_color(&wormStyle, LV_STATE_DEFAULT, LV_COLOR_LIME);
	lv_style_set_bg_opa(&wormStyle, LV_STATE_DEFAULT, LV_OPA_COVER);
	lv_style_set_bg_color(&wormStyle, LV_STATE_DEFAULT, LV_COLOR_LIME);

	lv_style_init(&foodStyle);
	lv_style_set_border_color(&foodStyle, LV_STATE_DEFAULT, LV_COLOR_RED);
	lv_style_set_bg_opa(&foodStyle, LV_STATE_DEFAULT, LV_OPA_COVER);
	lv_style_set_bg_color(&foodStyle, LV_STATE_DEFAULT, LV_COLOR_RED);

	// Init score object - weird bug with massive label
	score = lv_label_create(lv_scr_act(), nullptr);
	lv_obj_set_style_local_text_font(score, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_42);
	lv_obj_set_width(score, LV_HOR_RES);
	lv_label_set_text(score, "Score: 0000");
	lv_obj_align(score, lv_scr_act(), LV_ALIGN_IN_TOP_MID, 0, 0);
	scoreValue = 0;

	// Format grid for worm segments
	grid = lv_table_create(lv_scr_act(), nullptr);
	lv_obj_add_style(grid, LV_TABLE_PART_CELL1, &bgStyle);
	lv_obj_add_style(grid, LV_TABLE_PART_CELL2, &wormStyle);
	lv_obj_add_style(grid, LV_TABLE_PART_CELL3, &foodStyle);
	lv_table_set_col_cnt(grid, GRID_COL_MAX);
	lv_table_set_row_cnt(grid, GRID_ROW_MAX);
	for(int i = 0; i < 10; i++) {
		lv_table_set_col_width(grid, i, LV_HOR_RES / GRID_COL_MAX);
	}
	lv_obj_align(grid, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
	lv_obj_clean_style_list(grid, LV_TABLE_PART_BG);

	// Initialize grid
	for(int row = 0; row < GRID_ROW_MAX; row++) {
		for(int col = 0; col < GRID_COL_MAX; col++) {
			lv_table_set_cell_type(grid, row, col, 1); // 1 is default background style
			lv_table_set_cell_align(grid, row, col, LV_LABEL_ALIGN_CENTER);
		}
	}

	startTime = xTaskGetTickCount();
	placeFood();
	// Start game with three worm segments
	growWorm(0, 0);
	growWorm(0, 1);
	growWorm(0, 1);

	taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
}

Worm::~Worm()
{
	lv_task_del(taskRefresh);
	lv_style_reset(&wormStyle);
	lv_style_reset(&bgStyle);
	lv_obj_clean(lv_scr_act());
}

void Worm::Refresh()
{

    // Timer code stolen from Stopwatch app
    TickType_t currentTime = xTaskGetTickCount();
    TickType_t deltaTime = 0;
    deltaTime = (currentTime - startTime) & 0xFFFFFFFF;


    // TODO: Restart game on wall hit

	if(worm.front().y <= 0 || worm.front().y >= GRID_ROW_MAX - 1 ||
	   worm.front().x <= 0 || worm.front().x >= GRID_COL_MAX - 1) {
        restartGame();
	}

    //
    // Timer used so the worm doesn't move stupid fast
    if(deltaTime > 200){ // millis
        moveWorm();
        lv_obj_report_style_mod(&bgStyle);
        lv_obj_report_style_mod(&wormStyle);
        lv_obj_report_style_mod(&foodStyle);
        startTime = xTaskGetTickCount();
    }



	//moveWorm();


	/* Eat food if worm head overlaps food
	// Food repositioning happens in placeFood()
	if(worm.front().x <= food.x + SEG_SIZE/2 && worm.front().x >= food.x - SEG_SIZE/2){
	    if(worm.front().y <= food.y + SEG_SIZE/2 && worm.front().y >= food.y - SEG_SIZE/2){
	        switch(worm.back().currentDir){
	            case WORM_DIR::UP:
	                growWorm(0, SEG_SIZE);
	            break;
	            case WORM_DIR::RIGHT:
	                growWorm(-1*SEG_SIZE, 0);
	            break;
	            case WORM_DIR::DOWN:
	                growWorm(0, -1*SEG_SIZE);
	            break;
	            case WORM_DIR::LEFT:
	                growWorm(SEG_SIZE, 0);
	            break;
	        }

	        placeFood();
	        // Add points to score
	        scoreValue += FOOD_POINTS;
	        lv_label_set_text_fmt(score, "Score: %4d", scoreValue);

	    }
	}
	*/
}

bool Worm::OnTouchEvent(TouchEvents event)
{
	worm.front().redirectX = worm.front().x;
	worm.front().redirectY = worm.front().y;

	for(int i = 0; i < worm.size(); i++) {
		// Not the leader segment
		if(i > 0) {
			worm.at(i).redirectX = worm.at(i-1).redirectX;
			worm.at(i).redirectY = worm.at(i-1).redirectY;
		}

		// Update leader segment direction based on direction of swipe
		switch(event) {
		case TouchEvents::SwipeUp:
			if(worm.front().currentDir != WORM_DIR::DOWN)
				worm.front().currentDir = WORM_DIR::UP;
			break;

		case TouchEvents::SwipeRight:
			if(worm.front().currentDir != WORM_DIR::LEFT)
				worm.front().currentDir = WORM_DIR::RIGHT;
			break;

		case TouchEvents::SwipeDown:
			if(worm.front().currentDir != WORM_DIR::UP)
				worm.front().currentDir = WORM_DIR::DOWN;
			break;

		case TouchEvents::SwipeLeft:
			if(worm.front().currentDir != WORM_DIR::RIGHT)
				worm.front().currentDir = WORM_DIR::LEFT;
			break;
		}
	}

	return true;
}

void Worm::placeFood()
{
	// TODO: fix a potential issue with rand food positions spawning on top of the worm
	// TODO: fix an issue with food spawning at the edge of the border
	food.row = rand() % GRID_ROW_MAX;
	food.col = rand() % GRID_COL_MAX;
	lv_table_set_cell_type(grid, food.row, food.col, 3);

	std::cout << "Food placed" << std::endl;
}

void Worm::moveWorm()
{
    // Move worm
	for(int i = 0; i < worm.size(); i++) {
        // Underflow/overflow checking for borders
        if(worm.at(i).y < 1){ worm.at(i).y = 1; }
        if(worm.at(i).y > GRID_ROW_MAX - 1){ worm.at(i).y = GRID_ROW_MAX - 1; }
        if(worm.at(i).x < 1){ worm.at(i).x = 1; }
        if(worm.at(i).x > GRID_COL_MAX - 1){ worm.at(i).x = GRID_COL_MAX - 1; }

		switch(worm.at(i).currentDir) {
		case WORM_DIR::UP:
            //std::cout << "Before decrement i: " << i << " worm y: " << worm.at(i).y << std::endl;
			worm.at(i).y -= 1;
			//std::cout << "After decrement i: " << i << " worm y: " << worm.at(i).y << std::endl;

			// Set new position to the worm style
			lv_table_set_cell_type(grid, worm.at(i).y, worm.at(i).x, 2);
			// Set old position to the background style
			lv_table_set_cell_type(grid, worm.at(i).y + 1, worm.at(i).x, 1);
			break;
		case WORM_DIR::RIGHT:
			worm.at(i).x += 1;
			lv_table_set_cell_type(grid, worm.at(i).y, worm.at(i).x, 2);
			lv_table_set_cell_type(grid, worm.at(i).y, worm.at(i).x - 1, 1);
			break;
		case WORM_DIR::DOWN:
			worm.at(i).y += 1;
			lv_table_set_cell_type(grid, worm.at(i).y, worm.at(i).x, 2);
			lv_table_set_cell_type(grid, worm.at(i).y - 1, worm.at(i).x, 1);
			break;
		case WORM_DIR::LEFT:
			worm.at(i).x -= 1;
			lv_table_set_cell_type(grid, worm.at(i).y, worm.at(i).x, 2);
			lv_table_set_cell_type(grid, worm.at(i).y, worm.at(i).x + 1, 1);
			break;
		}
	}
}

void Worm::restartGame(){
    std::cout << "Restart Function" << std::endl;

    // Remove all but the worm head
    for(int i = worm.size()-1; i >= 1; i--){
        worm.pop_back();
    }

    // Initilize all cells back to the base black background
    for(int row = 0; row < GRID_ROW_MAX; row++){
        for(int col = 0; col < GRID_COL_MAX; col++){
            lv_table_set_cell_type(grid, row, col, 1);
        }
    }

    // Move the first segment back to the center start location
    worm.front().x = GRID_COL_MAX / 2;
    worm.front().y = GRID_ROW_MAX / 2 + 1;
    //worm.front().currentDir = WORM_DIR::UP;
    lv_table_set_cell_type(grid, GRID_COL_MAX / 2, GRID_ROW_MAX / 2 + 1, 2);
    lv_table_set_cell_type(grid, food.row, food.col, 3);
}

void Worm::growWorm(int16_t xOffset, int16_t yOffset)
{
	//std::cout << worm.size() << std::endl;
	struct WormSegment newWormSegment;

	if(worm.size() == 0) {
		newWormSegment.x = GRID_COL_MAX / 2;
		newWormSegment.y = GRID_ROW_MAX / 2 + 1;
		newWormSegment.currentDir = WORM_DIR::UP;
		worm.push_back(newWormSegment);
		lv_table_set_cell_type(grid, newWormSegment.y, newWormSegment.x, 2);
	}
	else {
		newWormSegment.x = worm.back().x + xOffset;
		newWormSegment.y = worm.back().y + yOffset;
		newWormSegment.currentDir = worm.back().currentDir;
		worm.push_back(newWormSegment);
		lv_table_set_cell_type(grid, newWormSegment.y, newWormSegment.x, 2);
	}
}
