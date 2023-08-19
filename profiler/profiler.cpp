/*
 * Copyright (C) 2013 Hermann Meyer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * --------------------------------------------------------------------------
 */

#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/stat.h>

#include <iomanip>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <climits>
#include <cstring>

#include <sndfile.hh>

////////////////////////////// LOCAL INCLUDES //////////////////////////

#include "profiler.h"

#include "profiler.cc"

static int ONLY_ONE_PROFILER_ALLOWED = 0;

////////////////////////////// PLUG-IN CLASS ///////////////////////////

class Profiler
{
private:
  LV2_URID_Map*        map;
  int32_t     rt_prio;
  int32_t     rt_policy;
  // pointer to buffer
  float*      output;
  float*      input;
  float*      output1;
  // pointer to dsp class
  profiler::Profil*  profil;
  // private functions
  inline void run_dsp_(uint32_t n_samples);
  inline void connect_(uint32_t port,void* data);
  inline void init_dsp_(uint32_t rate);
  inline void connect_all__ports(uint32_t port, void* data);
  inline void activate_f();
  inline void clean_up();
  inline void deactivate_f();

public:
  // LV2 Descriptor
  static const LV2_Descriptor descriptor[3];
  const LV2_ControlInputPort_Change_Request* ctrlInPortChangeReq;
  // static wrapper to private functions
  static void deactivate(LV2_Handle instance);
  static void cleanup(LV2_Handle instance);
  static void run(LV2_Handle instance, uint32_t n_samples);
  static void activate(LV2_Handle instance);
  static void connect_port(LV2_Handle instance, uint32_t port, void* data);
  static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                                double rate, const char* bundle_path,
                                const LV2_Feature* const* features);
  Profiler();
  ~Profiler();
};

// constructor
Profiler::Profiler() :
  rt_prio(0),
  rt_policy(0),
  output(NULL),
  input(NULL),
  ctrlInPortChangeReq(NULL) { };

// destructor
Profiler::~Profiler()
{
  profil->activate_plugin(false, profil);
  profil->delete_instance(profil);
};

///////////////////////// PRIVATE CLASS  FUNCTIONS /////////////////////

void Profiler::init_dsp_(uint32_t rate)
{
  profil = new profiler::Profil(1);
  profil->set_thread_prio(rt_prio, rt_policy);
  profil->ctrlInPortChangeReq = ctrlInPortChangeReq;
  profil->set_samplerate(rate, profil); // init the DSP class
}

// connect the Ports used by the plug-in class
void Profiler::connect_(uint32_t port,void* data)
{
  switch ((PortIndex)port)
    {
    case EFFECTS_OUTPUT:
      output = static_cast<float*>(data);
      break;
    case EFFECTS_INPUT:
      input = static_cast<float*>(data);
      break;
    default:
      break;
    }
}

void Profiler::activate_f()
{
  // allocate the internal DSP mem
  profil->activate_plugin(true, profil);
}

void Profiler::clean_up()
{
  // delete the internal DSP mem
  profil->activate_plugin(false, profil);
}

void Profiler::deactivate_f()
{
  // delete the internal DSP mem
  profil->activate_plugin(false, profil);
}

void Profiler::run_dsp_(uint32_t n_samples)
{
  profil->mono_audio(static_cast<int>(n_samples), input, output, profil);
}

void Profiler::connect_all__ports(uint32_t port, void* data)
{
  // connect the Ports used by the plug-in class
  connect_(port,data); 
  // connect the Ports used by the DSP class
  profil->connect_ports(port,  data, profil);
}

////////////////////// STATIC CLASS  FUNCTIONS  ////////////////////////

LV2_Handle 
Profiler::instantiate(const LV2_Descriptor* descriptor,
                            double rate, const char* bundle_path,
                            const LV2_Feature* const* features)
{
  if(ONLY_ONE_PROFILER_ALLOWED != 0) return NULL;
  ONLY_ONE_PROFILER_ALLOWED++;
  // init the plug-in class
  Profiler *self = new Profiler();
  if (!self)
    {
      return NULL;
    }
  
  const LV2_Options_Option* options  = NULL;

  for (int32_t i = 0; features[i]; ++i)
    {
      if (!std::strcmp(features[i]->URI, LV2_URID__map))
        {
          self->map = (LV2_URID_Map*)features[i]->data;
        } 
      else if (!std::strcmp(features[i]->URI, LV2_OPTIONS__options))
        {
          options = (const LV2_Options_Option*)features[i]->data;
        }
      else if (!std::strcmp(features[i]->URI, LV2_CONTROL_INPUT_PORT_CHANGE_REQUEST_URI))
        {
          self->ctrlInPortChangeReq = (const LV2_ControlInputPort_Change_Request*)features[i]->data;
        }

    }
  if (self->map)
    {
      if (options)
        {
          LV2_URID atom_Int  = self->map->map (self->map->handle, LV2_ATOM__Int);
          LV2_URID tshed_pol = self->map->map (self->map->handle, "http://ardour.org/lv2/threads/#schedPolicy");
          LV2_URID tshed_pri = self->map->map (self->map->handle, "http://ardour.org/lv2/threads/#schedPriority");
          for (const LV2_Options_Option* o = options; o->key; ++o)
            {
            if (o->context == LV2_OPTIONS_INSTANCE &&
                o->key == tshed_pol &&
                o->type == atom_Int)
              {
                self->rt_policy = *(const int32_t*)o->value;
              }
            if (o->context == LV2_OPTIONS_INSTANCE &&
                o->key == tshed_pri &&
                o->type == atom_Int)
              {
                self->rt_prio = *(const int32_t*)o->value;
              }
           }
        }
    }

#ifndef  __MOD_DEVICES__
  if (!self->ctrlInPortChangeReq)
    {
      fprintf(stderr, "Missing feature LV2_ControlInputPort_Change_Request.\n");
    }
#endif
  self->init_dsp_((uint32_t)rate);

  return (LV2_Handle)self;
}

void Profiler::connect_port(LV2_Handle instance, 
                                   uint32_t port, void* data)
{
  // connect all ports
  static_cast<Profiler*>(instance)->connect_all__ports(port, data);
}

void Profiler::activate(LV2_Handle instance)
{
  // allocate needed mem
  static_cast<Profiler*>(instance)->activate_f();
}

void Profiler::run(LV2_Handle instance, uint32_t n_samples)
{
  // run dsp
  static_cast<Profiler*>(instance)->run_dsp_(n_samples);
}

void Profiler::deactivate(LV2_Handle instance)
{
  // free allocated mem
  static_cast<Profiler*>(instance)->deactivate_f();
}

void Profiler::cleanup(LV2_Handle instance)
{
  // well, clean up after us
  Profiler* self = static_cast<Profiler*>(instance);
  self->clean_up();
  delete self;
}

const LV2_Descriptor Profiler::descriptor[] = {
    {
      SCPLUGIN_URI,
      Profiler::instantiate,
      Profiler::connect_port,
      Profiler::activate,
      Profiler::run,
      Profiler::deactivate,
      Profiler::cleanup,
      NULL
    },
};

////////////////////////// LV2 SYMBOL EXPORT ///////////////////////////

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index<1) {
        return &Profiler::descriptor[index];
    } else {
        return NULL;
    }
}

///////////////////////////// FIN //////////////////////////////////////
