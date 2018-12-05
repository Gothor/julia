#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <time.h>
#include "palettes.h" // Contient les palettes (attention, c'est brut...)

/* CONSTANTES *****************************************************************/

#define IMG_W 1024
#define IMG_H 1024 //768
#define MAX_NORM 4        // 2
#define STEP 0.05

/* GLOBALES *******************************************************************/

typedef enum {
  HUE = 0,
  BLACK_AND_WHITE,
  PALETTE1,
  PALETTE2,
  COLOR_MODES
} color_mode_t;

int nbThreads = 1;
int nbPartsPerThread = 1;
int nbParts = 1;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int nextPart = 0;
cv::Mat newImg(IMG_H, IMG_W, CV_8UC3, cv::Vec3b(0,0,0));
int partSize = 0;
bool keepGoing = true;
int offsetColor = 0;
color_mode_t colorMode = HUE;

long double offsetLeft = 0.0;
long double offsetTop = 0.0;
long double limitLeft = -1.0;
long double limitRight = 1.0;
long double limitTop = -1.0;
long double limitBottom = 1.0;

int maxIter = 300;
long double zoom = 1.0;

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
        ((long double) x / IMG_W * (limitRight - limitLeft) + limitLeft) * zoom + offsetLeft,
        ((long double) y / IMG_H * (limitBottom - limitTop) + limitTop) * zoom + offsetTop);
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
            int j = juliaDot(convert(x, y), maxIter);
            cv::Vec3b color(j, j, j);
            img.at<cv::Vec3b>(cv::Point(x, y)) = color;
        }
    }
}

/* MAIN ***********************************************************************/

cv::Vec3b hsv2bgr(int h, float s, float v) {
  int r, g, b;
  int ti = (h / 60) % 6;
  float f = h / 60.0 - ti;
  int l = 255 * v * (1 - s);
  int m = 255 * v * (1 - f * s);
  int n = 255 * v * (1 - (1 - f) * s);
  v *= 255;
  switch (ti) {
    case 0:
      return cv::Vec3b(l, n, v);
    case 1:
      return cv::Vec3b(l, v, m);
    case 2:
      return cv::Vec3b(n, v, l);
    case 3:
      return cv::Vec3b(v, m, l);
    case 4:
      return cv::Vec3b(v, l, n);
    default:
    case 5:
      return cv::Vec3b(m, l, v);
  }
}

void* child(void *arg) {
    pthread_mutex_lock(&mutex);
    while(1) {
      int part;
      while (nextPart >= nbParts && keepGoing) {
        pthread_cond_wait(&cond, &mutex);
      }

      if (!keepGoing) {
        pthread_mutex_unlock(&mutex);
        return NULL;
      }

      part = nextPart++;
      pthread_mutex_unlock(&mutex);

      for (int i = part * partSize; i < IMG_H * IMG_W && i < (part + 1) * partSize; i++) {
          int x = i % IMG_W;
          int y = i / IMG_W;

          int j = juliaDot(convert(x, y), maxIter);
          cv::Vec3b color;
          switch (colorMode) {
            case BLACK_AND_WHITE:
              newImg.at<cv::Vec3b>(cv::Point(x, y)) = cv::Vec3b(j, j, j);
              break;
            case PALETTE1:
              color = cv::Vec3b(palette[j*3], palette[j*3+1], palette[j*3+2]);
              newImg.at<cv::Vec3b>(cv::Point(x, y)) = color;
              break;
            case PALETTE2:
              color = cv::Vec3b(palette2[j*3], palette2[j*3+1], palette2[j*3+2]);
              newImg.at<cv::Vec3b>(cv::Point(x, y)) = color;
              break;
            case HUE:
            default:
              color = hsv2bgr(((j * 360) / 255 + offsetColor) % 360, 1.0, 1.0);
              newImg.at<cv::Vec3b>(cv::Point(x, y)) = color;
          }
      }
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char * argv[]) {
    int i;
    int v;
    float reel = -1.41702285618f;
    float imag = 0.0f;

    printf("Usage: %s [Number of threads [Number of parts per thread [Initial real part [Initial imaginary part]]]]\n"
      "- Number of threads: integer higher or equal to 1\n"
      "- Number of parts per thread: integer higher or equal to 1\n"
      "- Initial real part: float (real part of the complex number c used to compute the julia set)\n"
      "- Initial imaginary part: float (imaginary part of the complex number c used to compute the julia set)\n", argv[0]);
    printf("\n");
    printf("Commands:\n"
      "- LEFT and RIGHT arrows: move the camera horizontaly\n"
      "- UP and DOWN arrows; move the camera verticaly\n"
      "- r and f: decrease or increase the maximum number of iterations to compute the julia set\n"
      "- z and s: increase or decrease the imaginary part of the complex number c used to compute the julia set\n"
      "- d and q: increase or decrease the real part of the complex number c used to compute the julia set\n"
      "- t and g: increase or decrease the hue offset (only works with the initial colore mode)\n"
      "- e and a: zoom in or out\n"
      "- SPACE: switch between multiple color modes\n"
      "- w: save the current image\n"
      "- q: quit\n");

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
    
    c = new_complex(reel, imag);

    pthread_t tid[nbThreads];
    for (int j = 0; j < nbThreads; j++) {
        pthread_create(&tid[j], NULL, child, NULL);
    }

    while (keepGoing) {
      // printf("%Lf, %Lf\n", c.real, c.imag);

        // cv::cvtColor(newImg, newImg, cv::COLOR_HSV2BGR);
        imshow("image", newImg); // met à jour l'image
        int key = -1; // -1 indique qu'aucune touche est enfoncée

        if( (key = cv::waitKey(30)) != -1) {
          if (key == 81) { // Left key
            offsetLeft -= STEP * zoom;
          }
          else if (key == 83) { // Right key
            offsetLeft += STEP * zoom;
          }
          else if (key == 82) { // Up key
            offsetTop -= STEP * zoom;
          }
          else if (key == 84) { // Down key
            offsetTop += STEP * zoom;
          }
          else if (key == 'r' && maxIter > 10) {
            maxIter -= 10;
          }
          else if (key == 'f') {
            maxIter += 10;
          }
          else if (key == 'a') {
            zoom /= 0.5;
          }
          else if (key == 'e') {
            zoom *= 0.5;
          }
          else if (key == 'z') {
            c.imag += STEP;
          }
          else if (key == 's') {
            c.imag -= STEP;
          }
          else if (key == 'd') {
            c.real += STEP;
          }
          else if (key == 'q') {
            c.real -= STEP;
          }
          else if (key == 't') {
            offsetColor = (offsetColor + 1) % 360;
          }
          else if (key == 'g') {
            offsetColor--;
            while (offsetColor < 0)
              offsetColor += 360;
          }
          else if (key == 'w') {
            char name[100];
            time_t timer;
            struct tm* tm_info;
            time(&timer);
            tm_info = localtime(&timer);
            strftime(name, 100, "img_julia_%Y%m%d_%H%M%S.bmp", tm_info);
            imwrite(name, newImg);
            printf("Image saved\n");
          }
          else if (key == 32) { // Space
            colorMode = (color_mode_t) (((int) colorMode + 1) % (int) COLOR_MODES);
          }
          else if (key == 27) { // Escape
            keepGoing = false;
          }
          pthread_mutex_lock(&mutex);
          nextPart = 0;
          pthread_cond_broadcast(&cond);
          pthread_mutex_unlock(&mutex);
        }
    }
    cvDestroyWindow("image"); // ferme la fenêtre

    pthread_cond_broadcast(&cond);
    for (int j = 0; j < nbThreads; j++) {
        pthread_join(tid[j], NULL);
    }

    return 0;
}
