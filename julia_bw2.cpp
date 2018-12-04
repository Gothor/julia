#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>

/* CONSTANTES *****************************************************************/

#define IMG_W 1024
#define IMG_H 1024 //768
#define MAX_NORM 4        // 2

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
        ((long double) x / IMG_W * (limitRight - limitLeft)) * zoom + limitLeft * zoom,
        ((long double) y / IMG_H * (limitBottom - limitTop)) * zoom + limitTop * zoom);
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
      r = v;
      g = n;
      b = l;
      break;
    case 1:
      r = m;
      g = v;
      b = l;
      break;
    case 2:
      r = l;
      g = v;
      b = n;
      break;
    case 3:
      r = l;
      g = m;
      b = v;
      break;
    case 4:
      r = n;
      g = l;
      b = v;
      break;
    case 5:
      r = v;
      g = l;
      b = m;
      break;
  }
  return cv::Vec3b(b, g, r);
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

            int j = juliaDot(convert(x, y), maxIter);
            // cv::Vec3b color(j, 255, 255);
            cv::Vec3b color = hsv2bgr((j * 360) / 255, 1.0, 1.0);
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
    
    pthread_t tid[nbThreads];

    c = new_complex(reel, imag);
    bool keepGoing = true;
    while (keepGoing) {
      printf("%Lf, %Lf\n", c.real, c.imag);
        nextPart = 0;
        for (int j = 0; j < nbThreads; j++) {
            pthread_create(&tid[j], NULL, child, NULL);
        }

        for (int j = 0; j < nbThreads; j++) {
            pthread_join(tid[j], NULL);
        }

        // cv::cvtColor(newImg, newImg, cv::COLOR_HSV2BGR);
        imshow("image", newImg); // met à jour l'image
        int key = -1; // -1 indique qu'aucune touche est enfoncée

        while( (key = cv::waitKey(30)) ) {
          if (key == 81) { // Left key
            limitLeft -= 0.1 * zoom;
            limitRight -= 0.1 * zoom;
            break;
          }
          if (key == 83) { // Right key
            limitLeft += 0.1 * zoom;
            limitRight += 0.1 * zoom;
            break;
          }
          if (key == 82) { // Up key
            limitTop -= 0.1;
            limitBottom -= 0.1;
            break;
          }
          if (key == 84) { // Down key
            limitTop += 0.1;
            limitBottom += 0.1;
            break;
          }
          if (key == 'r' && maxIter > 10) {
            maxIter -= 10;
            break;
          }
          if (key == 'f') {
            maxIter += 10;
            break;
          }
          if (key == 'a') {
            zoom /= 0.5;
            break;
          }
          if (key == 'e') {
            zoom *= 0.5;
            break;
          }
          if (key == 'z') {
            c.imag += 0.1;
            break;
          }
          if (key == 's') {
            c.imag -= 0.1;
            break;
          }
          if (key == 'd') {
            c.real += 0.1;
            break;
          }
          if (key == 'q') {
            c.real -= 0.1;
            break;
          }
          if (key == 27) { // Escape
            keepGoing = false;
            break;
          }
        }
    }
    cvDestroyWindow("image"); // ferme la fenêtre

    return 0;
}
