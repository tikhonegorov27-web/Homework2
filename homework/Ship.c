#include <stdio.h>
#include <stdlib.h>
#include "lodepng.h" 

#define BRIGHTNESS_DIFFERENCE 30
#define White 255
#define Black 0
#define GAUSS_THRESHOLD 50 // Порог для бинаризации после размытия
#define MIN_TANKER_SIZE 8


unsigned char* load_png(const char* filename, unsigned int* width, unsigned int* height) {
    unsigned char* image = NULL;
    int error = lodepng_decode32_file(&image, width, height, filename);
    if (error != 0) {
        printf("error %u: %s\n", error, lodepng_error_text(error));
    }
    return image;
}


void write_png(const char* filename, const unsigned char* image, unsigned width, unsigned height) {
    unsigned char* png;
    size_t pngsize;
    int error = lodepng_encode32(&png, &pngsize, image, width, height);
    if (error == 0) {
        lodepng_save_file(png, pngsize, filename);
    } else {
        printf("error %u: %s\n", error, lodepng_error_text(error));
    }
    free(png);
}


int check_pixel(unsigned char *picture, int bw_size, int i, int neighbour) {
    /*
    Выделяет контрастные объекты на сером фоне
    */
    if (!picture) {
        return -1;
    }
    if (i >= 0 && i < bw_size && neighbour > 0 && neighbour < bw_size) {
        if (picture[i] - picture[neighbour] > BRIGHTNESS_DIFFERENCE) {
            return 1;
        }
    }
    return 0;
}


int turn_back(unsigned char *picture, unsigned char *bw_pic, int bw_size) {
    /*
    Преобразуем полученные координаты для записи в файл
    */
    if (!picture || !bw_pic) {
        return -1;
    }
    int i;
    for (i = 0; i < bw_size; i++) {
        picture[i * 4] = bw_pic[i];
        picture[i * 4 + 1] = bw_pic[i];
        picture[i * 4 + 2] = bw_pic[i];
        picture[i * 4 + 3] = White;
    }
    return 1;
}


void Gauss_blur(unsigned char *col, unsigned char *blr_pic, int width, int height)
{ 
    int i, j; 
    for(i=1; i < height-1; i++) 
        for(j=1; j < width-1; j++)
        { 
            blr_pic[width*i+j] = 0.084*col[width*i+j] + 0.084*col[width*(i+1)+j] + 0.084*col[width*(i-1)+j]; 
            blr_pic[width*i+j] = blr_pic[width*i+j] + 0.084*col[width*i+(j+1)] + 0.084*col[width*i+(j-1)]; 
            blr_pic[width*i+j] = blr_pic[width*i+j] + 0.063*col[width*(i+1)+(j+1)] + 0.063*col[width*(i+1)+(j-1)]; 
            blr_pic[width*i+j] = blr_pic[width*i+j] + 0.063*col[width*(i-1)+(j+1)] + 0.063*col[width*(i-1)+(j-1)]; 
        } 
   return; 
} 


void improve_contrast(unsigned char *bw_pic, int bw_size, int low_threshold, int high_threshold) {
    /*
    Улучшение контрастности
    */
    int i;
    for (i = 0; i < bw_size; i++) {
        if (bw_pic[i] < low_threshold) {
            bw_pic[i] = Black;
        } else if (bw_pic[i] > high_threshold) {
            bw_pic[i] = White;
        }
    }
}


int fill_rectangles(unsigned char* bw_pic, unsigned char* territory, int width) {
    if (!territory || !bw_pic) {
        return -1;
    }
    int x;
    int y;
    int sector;
    int sectors_amount = 13;
    /*
    Координаты верхнего левого и правого нижнего углов прямоугольников,
    в которых находятся танкеры. Данные из paint
    */
    int sectors[13][2][2] = {
        {{286, 20}, {356, 107}},
        {{534, 5}, {560,44}},
        {{642, 40}, {704, 252}},
        {{741, 35}, {748, 51}},
        {{554, 165}, {585, 270}},
        {{496, 302}, {551, 317}},
        {{551, 302}, {562, 340}},
        {{494, 327}, {551, 346}},
        {{509, 346}, {561, 422}},
        {{569, 276}, {823, 600}},
        {{534, 457}, {572, 560}},
        {{838, 534}, {1076, 641}},
        {{834, 265}, {1119, 510}}
    };
    
    for (sector = 0; sector < sectors_amount; sector++) {
        for (x = sectors[sector][0][0]; x < sectors[sector][1][0]; x++) {
            for (y = sectors[sector][0][1]; y < sectors[sector][1][1]; y++) {
                if (bw_pic[y * width + x]) {
                    territory[y * width + x] = White;
                }
            }
        }
    }
    return 1;
}


int dfs_recursive_with_size(
    unsigned char* territory,
    unsigned char* visited,
    int current,
    int bw_size,
    int width,
    int* size
) {
    /*
    Рекурсивный поиск в глубину с подсчетом размера области
     */
    if (current < 0 || 
        current >= bw_size || 
        visited[current] || 
        !territory[current]) {
        return 0;
    }
    
    visited[current] = 1;
    (*size)++;

    dfs_recursive_with_size(territory, visited, current + 1, bw_size, width, size);
    dfs_recursive_with_size(territory, visited, current - 1, bw_size, width, size);
    dfs_recursive_with_size(territory, visited, current + width, bw_size, width, size);
    dfs_recursive_with_size(territory, visited, current - width, bw_size, width, size);
    dfs_recursive_with_size(territory, visited, current + width + 1, bw_size, width, size);
    dfs_recursive_with_size(territory, visited, current + width - 1, bw_size, width, size);
    dfs_recursive_with_size(territory, visited, current - width + 1, bw_size, width, size);
    dfs_recursive_with_size(territory, visited, current - width - 1, bw_size, width, size);
    
    return 1;
}


int set_contrast(int bw_size, unsigned char* bw_pic, unsigned char* picture, int width) {
    /*
    Повышаем контраст изображения, проверяя соседей
    */
    if (!bw_pic || !picture) {
        return -1;
    }
    int i;
    unsigned char value;

    for (i = 0; i < bw_size; i++) {
        value = (picture[i * 4] + picture[i * 4 + 1] + picture[i * 4 + 2]) / 3;
        bw_pic[i] = value;
    }

    for (i = 0; i < bw_size; i++) {
        if (
            check_pixel(bw_pic, bw_size, i, i - 1) ||
            check_pixel(bw_pic, bw_size, i, i + 1) ||
            check_pixel(bw_pic, bw_size, i, i - width) ||
            check_pixel(bw_pic, bw_size, i, i + width)
        ) {
            bw_pic[i] = White;
        }
    }

    for (i = 0; i < bw_size; i++) {
        if (bw_pic[i] < White) {
            bw_pic[i] = Black;
        }
    }
    return 1;
}



int main(void) {
    const char* filename = "prevPhoto.png";
    unsigned int width, height;
    int bw_size;
    unsigned char* picture = load_png(filename, &width, &height);
    int tankers_amount = 0;
    int i;
    
    if (picture == NULL) {
        printf("Problem reading picture from the file %s. Error.\n", filename); 
        return -1; 
    }
    
    bw_size = width * height;

    unsigned char* bw_pic = calloc(bw_size, sizeof(unsigned char));
    unsigned char* blurred = calloc(bw_size, sizeof(unsigned char));
    unsigned char* visited = calloc(bw_size, sizeof(unsigned char));
    unsigned char* territory = calloc(bw_size, sizeof(unsigned char));
    
    set_contrast(bw_size, bw_pic, picture, width);
    
    Gauss_blur(bw_pic, blurred, width, height);

    improve_contrast(blurred, bw_size, 0, GAUSS_THRESHOLD);
    
    turn_back(picture, blurred, bw_size);
    write_png("contrast.png", picture, width, height);

    fill_rectangles(blurred, territory, width);
    
    for (i = 0; i < bw_size; i++) {
        if (!visited[i] && territory[i]) {
            int component_size = 0;
            dfs_recursive_with_size(territory, visited, i, bw_size, width, &component_size);
            
            if (component_size >= MIN_TANKER_SIZE) {
                tankers_amount++;
            }
        }
    }
    
    turn_back(picture, territory, bw_size);
    write_png("result.png", picture, width, height);
    
    printf("\n================================\n");
    printf("RESULT: Amount of tankers: %d\n", tankers_amount);
    printf("================================\n");

    free(bw_pic); 
    free(blurred);
    free(territory);
    free(visited);
    free(picture);
    
    return 0; 
}