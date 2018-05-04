#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>

// Import C headers for rust API
#include <pthread.h>
#include "myrustlib/gifski.h"

/* data to pass to encoder thread */
typedef struct {
  const char * path;
  gifski * g;
} gifski_encoder_thread_info;

/* gifski_write() blocks until main thread calls gifski_end_adding_frames() */
void * gifski_encoder_thread(void * data){
  gifski_encoder_thread_info * info = data;
  gifski_write(info->g, info->path);
  gifski_drop(info->g);
  return NULL;
}

SEXP R_png_to_gif(SEXP png_files, SEXP gif_file, SEXP width, SEXP height, SEXP delay, SEXP loop){
  if(!Rf_isString(png_files))
    Rf_error("png_files must be character vector");

  GifskiSettings settings;
  settings.height = Rf_asInteger(height);
  settings.width = Rf_asInteger(width);
  settings.quality = 100;
  settings.fast = false;
  settings.once = !Rf_asLogical(loop);

  /* init in main thread */
  gifski * g = gifski_new(&settings);

  /* create encoder thread; TODO: maybe we can use multiple encoder threads? */
  pthread_t encoder_thread;
  gifski_encoder_thread_info info = {CHAR(STRING_ELT(gif_file, 0)), g};
  if(pthread_create(&encoder_thread, NULL, gifski_encoder_thread, &info))
    Rf_error("Failed to create encoder thread");

  /* add png frames in main thread */
  for(size_t i = 0; i < Rf_length(png_files); i++){
    if(gifski_add_frame_png_file(g, i, CHAR(STRING_ELT(png_files, i)), Rf_asInteger(delay)) != GIFSKI_OK)
      Rprintf("Failed to add frame %d\n", i);
  }

  /* This will finalize the encoder thread as well */
  gifski_end_adding_frames(g);

  /* wait for the encoder thread to finish */
  if(pthread_join(encoder_thread, NULL))
    Rf_error("Failed to join encoder_thread");
  return gif_file;
}

// Standard R package stuff
static const R_CallMethodDef CallEntries[] = {
  {"R_png_to_gif", (DL_FUNC) &R_png_to_gif, 6},
  {NULL, NULL, 0}
};

void R_init_gifski(DllInfo *dll) {
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
}