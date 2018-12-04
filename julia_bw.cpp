#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>

/* CONSTANTES *****************************************************************/

#define IMG_W 1024
#define IMG_H 1024 //768
#define MAX_ITER 300    // 200
#define MAX_NORM 4        // 2

#define LIMIT_LEFT -1
#define LIMIT_RIGHT 1
#define LIMIT_TOP -1
#define LIMIT_BOTTOM 1

/* MANIPULER LES NOMBRES COMPLEXES ********************************************/

typedef struct {
    long double real;
    long double imag;
} complex;
complex c; // GLOBALE

complex new_complex(long double real, long double imag) {
    complex c;
    c.real = real;
    c.imag = imag;
    return c;
}

complex add_complex(complex a, complex b) {
    a.real += b.real;
    a.imag += b.imag;
    return a;
}

complex mult_complex(complex a, complex b) {
    complex m;
    m.real = a.real * b.real - a.imag * b.imag;
    m.imag = a.real * b.imag + a.imag * b.real;
    return m;
}

long double module_complex(complex c) {
    return c.real * c.real + c.imag * c.imag;
}

/* FRACTALE DE JULIA *****************************************************/

complex convert(int x, int y) {
   return new_complex(
        ((long double) x / IMG_W * (LIMIT_RIGHT - LIMIT_LEFT)) + LIMIT_LEFT,
        ((long double) y / IMG_H * (LIMIT_BOTTOM - LIMIT_TOP)) + LIMIT_TOP );
}

int juliaDot(complex z, int iter) {
	int i;
    for (i = 0; i < iter; i++) {
        z = add_complex(mult_complex(z, z), c);
        long double norm = module_complex(z);
        if (norm > MAX_NORM) {
            break;
        }
    }
    return i * 255 / iter; // on met i dans l'intervalle 0 à 255
}

void julia(cv::Mat& img) {
    for (int x = 0; x < IMG_W; x++) {
        for (int y = 0; y < IMG_H; y++) {
            int j = juliaDot(convert(x, y), MAX_ITER);
            cv::Vec3b color(j, j, j);
            img.at<cv::Vec3b>(cv::Point(x, y)) = color;
        }
    }
}

/* MAIN ***********************************************************************/

int nbThreads = 1;
int nbPartsPerThread = 1;
int nbParts = 1;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int nextPart = 0;
cv::Mat newImg(IMG_H, IMG_W, CV_8UC3, cv::Vec3b(0,0,0));
int partSize = 0;

void* child(void *arg) {
    // cv::Mat& img = newImg;

    pthread_mutex_lock(&mutex);
    while (nextPart < nbParts) {
        int part = nextPart++;
        pthread_mutex_unlock(&mutex);

        for (int i = part * partSize; i < IMG_H * IMG_W && i < (part + 1) * partSize; i++) {
            int x = i % IMG_W;
            int y = i / IMG_W;

            int j = juliaDot(convert(x, y), MAX_ITER);
            cv::Vec3b color(j, j, j);
            newImg.at<cv::Vec3b>(cv::Point(x, y)) = color;
        }

        pthread_mutex_lock(&mutex);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char * argv[]) {
    int i;
    int v;
    float reel = -1.41702285618f;
    float imag = 0.0f;

    // Récupération des paramètres;
    if (argc > 1) {
      nbThreads = atoi(argv[1]);
    }
    if (argc > 2) {
      nbPartsPerThread = atoi(argv[2]);
    }
    if (argc > 3) {
      reel = atof(argv[3]);
    }
    if (argc > 4) {
      imag = atof(argv[4]);
    }

    nbParts = nbThreads * nbPartsPerThread;
    
    partSize = IMG_H * IMG_W;
    if (partSize % nbParts != 0) {
        partSize += nbParts - partSize % nbParts;
    }
    partSize /= nbParts;

    /*
    printf("Number of threads : %d\n", nbThreads);
    printf("Number of parts per thread : %d\n", nbPartsPerThread);
    printf("Number of parts : %d\n", nbParts);
    printf("Image size : %d\n", IMG_W * IMG_H);
    printf("Part size : %d\n", partSize);
    printf("Reel : %f\n", reel);
    printf("Imag : %f\n", imag);
    */
    
    pthread_t tid[nbThreads];

    c = new_complex(reel, imag);

    for (int j = 0; j < nbThreads; j++) {
        pthread_create(&tid[j], NULL, child, NULL);
    }

    for (int j = 0; j < nbThreads; j++) {
        pthread_join(tid[j], NULL);
    }

    return 0;
}
