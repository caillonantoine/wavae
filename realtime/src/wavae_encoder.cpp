#include "../libwavae/src/deepAudioEngine.h"
#include "m_pd.h"
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <stdio.h>
#include <thread>
#include <vector>

#define LATENT_NUMBER 16
#define BUFFER_SIZE 4096

static t_class *wavae_encoder_tilde_class;

typedef struct _wavae_encoder_tilde {
  t_object x_obj;
  t_sample f;

  DeepAudioEngine *model;
  std::vector<float *> in_buffer, out_buffer;
  std::thread *worker;

  t_outlet *x_out[LATENT_NUMBER];
} t_wavae_encoder_tilde;

void perform(t_wavae_encoder_tilde *x, int n_signal) {
  x->model->perform(x->in_buffer, x->out_buffer, x->in_buffer.size(),
                    x->out_buffer.size(), n_signal);
}

t_int *wavae_encoder_tilde_perform(t_int *w) {

  t_wavae_encoder_tilde *x = (t_wavae_encoder_tilde *)w[1];
  std::vector<float *> input, output;

  // WAIT FOR THREAD TO END
  if (x->worker) {
    x->worker->join();
  }

  // PUT INPUT AND OUT BUFFER INTO VECTORS
  input.push_back((float *)(w[2]));
  for (int i(0); i < LATENT_NUMBER; i++) {
    output.push_back((float *)(w[i + 3]));
  }

  int n = (int)(w[LATENT_NUMBER + 3]);

  // COPY PREVIOUS COMPUTATIONS
  for (int d(0); d < LATENT_NUMBER; d++) {
    for (int t(0); t < n; t++) {
      output[d][t] = x->out_buffer[d][t];
    }
  }

  // COPY CURRENT BUFFER INTO X
  for (int t(0); t < n; t++) {
    x->in_buffer[0][t] = input[0][t];
  }

  // START THREAD TO COMPUTE NEXT BUFFER
  x->worker = new std::thread(perform, x, n);

  return (w + LATENT_NUMBER + 4);
}

void wavae_encoder_tilde_dsp(t_wavae_encoder_tilde *x, t_signal **sp) {
  dsp_add(wavae_encoder_tilde_perform, 19, x, sp[0]->s_vec, sp[1]->s_vec,
          sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec,
          sp[7]->s_vec, sp[8]->s_vec, sp[9]->s_vec, sp[10]->s_vec,
          sp[11]->s_vec, sp[12]->s_vec, sp[13]->s_vec, sp[14]->s_vec,
          sp[15]->s_vec, sp[16]->s_vec, sp[0]->s_n);
}

void wavae_encoder_tilde_free(t_wavae_encoder_tilde *x) {
  for (int i(0); i < LATENT_NUMBER; i++) {
    outlet_free(x->x_out[i]);
  }
}

void *wavae_encoder_tilde_new(t_floatarg *f) {
  t_wavae_encoder_tilde *x =
      (t_wavae_encoder_tilde *)pd_new(wavae_encoder_tilde_class);

  x->worker = NULL;

  // INITIALIZATION OF THE LATENT_NUMBER LATENT OUTPUTS
  for (int i(0); i < LATENT_NUMBER; i++) {
    x->x_out[i] = outlet_new(&x->x_obj, &s_signal);
  }

  // INITIALIZE BUFFER
  x->in_buffer.push_back(new float[BUFFER_SIZE]);
  for (int i(0); i < LATENT_NUMBER; i++) {
    x->out_buffer.push_back(new float[BUFFER_SIZE]);
  }

  // LOAD LIBWAVAE.SO ///////////////////////////////////
  void *hndl = dlopen("./libwavae/libwavae.so", RTLD_LAZY);
  if (!hndl) {
    std::cout << "Failed to load libwavae..." << std::endl;
  }

  void *symbol = dlsym(hndl, "get_encoder");
  if (!symbol) {
    std::cout << "Could not find symbol..." << std::endl;
  }
  auto build_encoder = reinterpret_cast<DeepAudioEngine *(*)()>(symbol);

  x->model = (*build_encoder)();
  int error = x->model->load("trace_model.ts");

  if (error) {
    std::cout << "could not load model" << std::endl;
  } else {
    std::cout << "model loaded successfuly!" << std::endl;
  }
  return (void *)x;
}

extern "C" {
void wavae_encoder_tilde_setup(void) {
  wavae_encoder_tilde_class =
      class_new(gensym("wavae_encoder~"), (t_newmethod)wavae_encoder_tilde_new,
                0, sizeof(t_wavae_encoder_tilde), CLASS_DEFAULT, A_DEFFLOAT, 0);

  class_addmethod(wavae_encoder_tilde_class, (t_method)wavae_encoder_tilde_dsp,
                  gensym("dsp"), A_CANT, 0);
  CLASS_MAINSIGNALIN(wavae_encoder_tilde_class, t_wavae_encoder_tilde, f);
}
}