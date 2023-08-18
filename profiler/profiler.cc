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

#include <libgen.h>
#include <stdio.h>
#include <dlfcn.h>
#include <fstream>

namespace profiler {

#define fmax(x, y) (((x) > (y)) ? (x) : (y))
#define fmin(x, y) (((x) < (y)) ? (x) : (y))

#define always_inline inline __attribute__((always_inline))

#define MAXRECSIZE 102400  //100kb
#define MAXFILESIZE INT_MAX-MAXRECSIZE // 2147352576  //2147483648-MAXRECSIZE

/*
 * Class MTDM is taken from jack_iodelay to measure the roundtrip latency
 * https://github.com/jackaudio/jack-example-tools/blob/main/tools/iodelay.cpp
 */
// --------------------------------------------------------------------------------

struct Freq
{
    int   p;
    int   f;
    float xa;
    float ya;
    float x1;
    float y1;
    float x2;
    float y2;
};


struct MTDM
{
    double  _del;
    double  _err;
    float   _wlp;
    int     _cnt;
    int     _inv;

    struct Freq _freq [13];
};


struct MTDM * mtdm_new (double fsamp)
{
    int   i;
    struct Freq  *F;

    struct MTDM *retval = (MTDM *)malloc( sizeof(struct MTDM) );

    if (retval==NULL)
        return NULL;

    retval->_cnt = 0;
    retval->_inv = 0;

    retval->_freq [0].f  = 4096;
    retval->_freq [1].f  = 2048;
    retval->_freq [2].f  = 3072;
    retval->_freq [3].f  = 2560;
    retval->_freq [4].f  = 2304;
    retval->_freq [5].f  = 2176; 
    retval->_freq [6].f  = 1088;
    retval->_freq [7].f  = 1312;
    retval->_freq [8].f  = 1552;
    retval->_freq [9].f  = 1800;
    retval->_freq [10].f = 3332;
    retval->_freq [11].f = 3586;
    retval->_freq [12].f = 3841;
    retval->_wlp = 200.0f / fsamp;
    for (i = 0, F = retval->_freq; i < 13; i++, F++) {
        F->p = 128;
        F->xa = F->ya = 0.0f;
        F->x1 = F->y1 = 0.0f;
        F->x2 = F->y2 = 0.0f;
    }

    return retval;
}

int mtdm_process (struct MTDM *self, size_t len, float *ip, float *op)
{
    int    i;
    float  vip, vop, a, c, s;
    struct Freq   *F;

    while (len--)
    {
        vop = 0.0f;
        vip = *ip++;
        for (i = 0, F = self->_freq; i < 13; i++, F++)
        {
            a = 2 * (float) M_PI * (F->p & 65535) / 65536.0; 
            F->p += F->f;
            c =  cosf (a); 
            s = -sinf (a); 
            vop += (i ? 0.01f : 0.20f) * s;
            F->xa += s * vip;
            F->ya += c * vip;
        } 
        *op++ = vop;
        if (++self->_cnt == 16)
        {
            for (i = 0, F = self->_freq; i < 13; i++, F++)
            {
                F->x1 += self->_wlp * (F->xa - F->x1 + 1e-20);
                F->y1 += self->_wlp * (F->ya - F->y1 + 1e-20);
                F->x2 += self->_wlp * (F->x1 - F->x2 + 1e-20);
                F->y2 += self->_wlp * (F->y1 - F->y2 + 1e-20);
                F->xa = F->ya = 0.0f;
            }
            self->_cnt = 0;
        }
    }

    return 0;
}

int mtdm_resolve (struct MTDM *self)
{
    int     i, k, m;
    double  d, e, f0, p;
    struct Freq *F = self->_freq;

    if (hypot (F->x2, F->y2) < 0.001) return -1;
    d = atan2 (F->y2, F->x2) / (2 * M_PI);
    if (self->_inv) d += 0.5;
    if (d > 0.5) d -= 1.0;
    f0 = self->_freq [0].f;
    m = 1;
    self->_err = 0.0;
    for (i = 0; i < 12; i++)
    {
        F++;
        p = atan2 (F->y2, F->x2) / (2 * M_PI) - d * F->f / f0;
        if (self->_inv) p += 0.5;
        p -= floor (p);
        p *= 2;
        k = (int)(floor (p + 0.5));
        e = fabs (p - k);
        if (e > self->_err) self->_err = e;
        if (e > 0.4) return 1; 
        d += m * (k & 1);
        m *= 2;
    }  
    self->_del = 16 * d;

    return 0;
}

void mtdm_invert (struct MTDM *self)
{
    self->_inv ^= 1;
}

// --------------------------------------------------------------------------------


class Profil {
private:
    SNDFILE *       recfile;
    SNDFILE *       playfile;
    std::string     inputfile;
    struct MTDM     *mtdm;
    int             fSamplingFreq;
    int             channel;
    float           *fcheckbox0;
    float           *fcheckbox1;
    float           *fbargraph;
    float           *fbargraph1;
    float           *errors;
    float           errors_;
    int             latency;
    int             roundtrip;
    int             measure;
    int             finish;
    int             IOTA;
    int             IOTAP;
    int             iA;
    int             savesize;
    int             filesize;
    int             inputsize;
    float           *fRec0;
    float           *fRec1;
    float           *tape;
    float           *tape1;
    sem_t           m_trig;
    pthread_t       m_pthr;
    int32_t         rt_prio;
    int32_t         rt_policy;
    volatile bool   keep_stream;
    bool            mem_allocated;
    bool            err;
    float           fConst0;
    float           fConst1;
    float           fConst2;
    float           nf;
    float           fRecb0[2];
    int             iRecb1[2];
    float           fRecb2[2];
    float           fRecb0r[2];
    int             iRecb1r[2];
    float           fRecb2r[2];

    void        mem_alloc();
    void        mem_free();
    void        clear_state_f();
    int         activate(bool start);
    void        init(unsigned int samplingFreq);
    void        compute(int count, float *input0, float *output0);
    void        save_to_wave(SNDFILE * sf, float *tape, int lSize);
    SNDFILE     *open_stream(std::string fname);
    void        close_stream(SNDFILE **sf);
    void        stop_thread();
    void        start_thread();
    void        disc_stream();
    void        connect(uint32_t port,void* data);
    void        normalize();
    static void *run_thread(void* p);
    inline int  load_from_wave(std::string fname);
    int         requestParameterValueChange(const uint32_t index, const float value);
    static bool requestParameterValueChangeCallback(void* const ptr, const uint32_t index, const float value);
    inline std::string get_path(); 
    inline std::string get_ffilename(); 
    inline std::string get_ifilename(); 

public:
    const LV2_ControlInputPort_Change_Request* ctrlInPortChangeReq;
    void        set_thread_prio(int32_t prio, int32_t policy);
    static void clear_state(Profil*);
    static int  activate_plugin(bool start, Profil*);
    static void set_samplerate(unsigned int samplingFreq, Profil*);
    static void mono_audio(int count, float *input0, float *output0, Profil*);
    static void delete_instance(Profil *p);
    static void connect_ports(uint32_t port,void* data, Profil *p);
    Profil(int channel_);
    ~Profil();
};

template <class T>
inline std::string to_string(const T& t) {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(3) << t;
    return ss.str();
}

Profil::Profil(int channel_)
    : recfile(NULL),
      playfile(NULL),
      channel(channel_),
      fRec0(0),
      fRec1(0),
      tape(fRec0),
      tape1(NULL),
      m_pthr(0),
      rt_prio(0),
      rt_policy(0),
      keep_stream(false),
      mem_allocated(false),
      err(false),
      ctrlInPortChangeReq(NULL) {
    sem_init(&m_trig, 0, 0);
}

Profil::~Profil() {
    stop_thread();
    free(mtdm);
    activate(false);
}

// get the path were we are installed
std::string get_profile_library_path() {
    Dl_info dl_info;
    if(0 != dladdr((void*)get_profile_library_path, &dl_info))
        return std::string(dl_info.dli_fname);
    else
        return std::string();
}

// get the path were to save the recording and the input file
inline std::string Profil::get_path() {
    struct stat sb;
    std::string pPath;

#ifndef  __MOD_DEVICES__
    pPath = getenv("HOME");
    pPath +="/profiles/";

#else
    pPath = "/data/user-files/Audio Recordings/profiles/";
#endif
    if (!(stat(pPath.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))) {
        mkdir(pPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }

    return pPath;
}

// get the recording path and filename
inline std::string Profil::get_ffilename() {
    return get_path() + "target.wav";
}

// check if input.wav is in path, otherwise copy it over and return path + filename
inline std::string Profil::get_ifilename() {
    struct stat sb;
    std::string iname = dirname(get_profile_library_path().data());
    iname += "/resources/input.wav";
    std::string oname = get_path() + "input.wav";
    if (stat (oname.c_str(), &sb) != 0) {
        std::ifstream src(iname.c_str(), std::ios::binary);
        std::ofstream dest(oname.c_str(), std::ios::binary);
        dest << src.rdbuf();
    }
    return oname;
}

// load wav file into buffer
inline int Profil::load_from_wave(std::string fname) {
    SF_INFO sfinfo;
    int f,c;
    int fSize = 0;
    sfinfo.format = 0;
    SNDFILE *sf = sf_open(fname.c_str(),SFM_READ,&sfinfo);
    if (sf ) {
        f = sfinfo.frames;
        c = sfinfo.channels;
        inputsize = f*c;

        try {
            tape1 = new float[inputsize]{};
        } catch(...) {
            err = true;
            return 0;
        }

        fSize = sf_read_float(sf, tape1, inputsize);
    }
    sf_close(sf);
    return fSize;
}

// save the chunks to disk
void Profil::disc_stream() {
    for (;;) {
        sem_wait(&m_trig);
        if (!recfile) {
            std::string fname = get_ffilename();
            recfile = open_stream(fname);
        }
        save_to_wave(recfile, tape, savesize);
        filesize +=savesize;
        if ((!keep_stream && recfile) || (filesize >MAXFILESIZE)) {
            close_stream(&recfile);
            filesize = 0;
        }
    }
}

// get priority and policity from host
void Profil::set_thread_prio(int32_t prio, int32_t policy) {
    rt_prio = prio;
    rt_policy = policy;
}

// run the recording thread
void *Profil::run_thread(void *p) {
    (reinterpret_cast<Profil *>(p))->disc_stream();
    return NULL;
}

// stop the recording thread
void Profil::stop_thread() {
    pthread_cancel (m_pthr);
    pthread_join (m_pthr, NULL);
}

// start the recording thread
void Profil::start_thread() {
    pthread_attr_t      attr;
    struct sched_param  spar;
    if (rt_prio == 0) {
        rt_prio = sched_get_priority_max(SCHED_FIFO);
    }
    if ((rt_prio/5) > 0) rt_prio = rt_prio/5;
    spar.sched_priority = rt_prio;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE );
    pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
    if (rt_policy == 0) {
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    } else {
        pthread_attr_setschedpolicy(&attr, rt_policy);
    }
    pthread_attr_setschedparam(&attr, &spar);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    // pthread_attr_setstacksize(&attr, 0x10000);
    if (pthread_create(&m_pthr, &attr, run_thread,
                       reinterpret_cast<void*>(this))) {
        err = true;
    }
    pthread_attr_destroy(&attr);
}

// clear all internal buffers on activation
inline void Profil::clear_state_f() {
    for (int i=0; i<MAXRECSIZE; i++) fRec0[i] = 0;
    for (int i=0; i<MAXRECSIZE; i++) fRec1[i] = 0;
    for (int i=0; i<2; i++) fRecb0[i] = 0;
    for (int i=0; i<2; i++) iRecb1[i] = 0;
    for (int i=0; i<2; i++) fRecb2[i] = 0.0000003; // -130db
    for (int i=0; i<2; i++) fRecb0r[i] = 0;
    for (int i=0; i<2; i++) iRecb1r[i] = 0;
    for (int i=0; i<2; i++) fRecb2r[i] = 0.0000003; // -130db
}

// static wrapper for internal clear_state call
void Profil::clear_state(Profil *p) {
    static_cast<Profil*>(p)->clear_state_f();
}

// init internal variables and the round trip mesuarment function
inline void Profil::init(unsigned int samplingFreq) {
    fSamplingFreq = samplingFreq;
    IOTA = 0;
    IOTAP = 0;
    latency = 0;
    roundtrip = 0;
    measure = 0;
    finish = 0;
    fConst1 = 0.1;
    fConst2 = 0.1;
    nf = 1.0;
    fConst0 = (1.0f / float(fmin(192000, fmax(1, fSamplingFreq))));
    mtdm = mtdm_new(fSamplingFreq);
    start_thread();
}

// static wrapper for the internal init call
void Profil::set_samplerate(unsigned int samplingFreq, Profil *p) {
    static_cast<Profil*>(p)->init(samplingFreq);
}

// save a chunk of data to a wave file
inline void Profil::save_to_wave(SNDFILE * sf, float *tape, int lSize) {
    if (sf) {
        sf_write_float(sf,tape, lSize);
        sf_write_sync(sf);
    } else {
        err = true;
    }
}

// open a wave file to write data in
SNDFILE *Profil::open_stream(std::string fname) {
    SF_INFO sfinfo ;
    sfinfo.channels = channel;
    sfinfo.samplerate = fSamplingFreq;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
    
    SNDFILE * sf = sf_open(fname.c_str(), SFM_WRITE, &sfinfo);
    if (sf) return sf;
    else return NULL;
}

// crude normalisation function, barly used
void Profil::normalize() {
    if (tape1) { delete tape1; tape1 = 0; }
    int n = load_from_wave(get_ffilename());
    for (int i = 0;i<n;i++) {
        tape1[i] *= nf;
    }
    SNDFILE * sf = open_stream(get_ffilename());
    if (sf) {
        save_to_wave(sf, tape1, n);
        sf_close(sf);
    }
    if (tape1) { delete tape1; tape1 = 0; }
    load_from_wave(inputfile);
}

// close wav file when last chunk is written
inline void Profil::close_stream(SNDFILE **sf) {
    if (*sf) sf_close(*sf);
    *sf = NULL;
    if (std::fabs(nf - 1.0) > 0.01) normalize();
}

// allocate the internal recording buffers
void Profil::mem_alloc() {
    if (!fRec0) fRec0 = new float[MAXRECSIZE];
    if (!fRec1) fRec1 = new float[MAXRECSIZE];
    mem_allocated = true;
}

// free the internal recording and play buffers
void Profil::mem_free() {
    mem_allocated = false;
    if (tape1) { delete tape1; tape1 = 0; }
    if (fRec0) { delete fRec0; fRec0 = 0; }
    if (fRec1) { delete fRec1; fRec1 = 0; }
}

// activate the plug
int Profil::activate(bool start) {
    if (start) {
        if (!mem_allocated) {
            mem_alloc();
            inputfile = get_ifilename();
            load_from_wave(inputfile);
            clear_state_f();
        }
    } else if (mem_allocated) {
        mem_free();
    }
    return 0;
}

// static wrapper for internal activate call
int Profil::activate_plugin(bool start, Profil *p) {
    (p)->activate(start);
    return 0;
}

// request the host to change a input parameter value
int Profil::requestParameterValueChange(const uint32_t index, const float value) {
    if (ctrlInPortChangeReq == nullptr)
        return 1;
    return ctrlInPortChangeReq->request_change(ctrlInPortChangeReq->handle, index, value);
}

// same, but return a bool instead error codes
bool Profil::requestParameterValueChangeCallback(void* const ptr, const uint32_t index, const float value) {
    return (((Profil*)ptr)->requestParameterValueChange(index, value) == 0);
}

// the process 
void always_inline Profil::compute(int count, float *input0, float *output0) {
    if (err) *fcheckbox0 = 0.0;
    int iSlow0 = finish ? 0 : int(*fcheckbox0);
    *fcheckbox1 = int(fRecb2[0]);
    if (!(*fcheckbox0)) {
        finish = 0;
        *errors = 0.0;
    }

    // measure roundtrip latency, running 20 frames
    if (iSlow0 && !roundtrip) {
        mtdm_process (mtdm, count, input0, output0);
        measure++;
        if (measure < 20) return;
    }
    // resolve roundtrip latency after 20 frames
    if (measure && !roundtrip) {
        // no signal comes in, stop the process here
        if (mtdm_resolve (mtdm) < 0) {
            roundtrip = 0;
            measure = 0;
            finish = 1;
            *errors = 1.0;
            requestParameterValueChange((PortIndex)PROFILE, 0.0f);
            //fprintf (stderr, "no signal comes in, stop the process here\n");
            return;
        }
        // when phase is inverted resolve with inverted frames
        if (mtdm->_err > 0.3) {
            mtdm_invert ( mtdm );
            mtdm_resolve ( mtdm );
        }
        // set roundtrip latency
        roundtrip = mtdm->_del;
        //printf ("roundtrip latency is %i\n", roundtrip);

        // seems we receive garbage, stop the process here
        if (mtdm->_err > 0.2) {
            roundtrip = 0;
            measure = 0;
            finish = 1;
            *errors = 2.0;
            requestParameterValueChange((PortIndex)PROFILE, 0.0f);
            //fprintf (stderr, "seems we receive garbage, stop the process here\n");
            return;
        }
    }
    for (int i=0; i<count; i++) {
        // default output is zero
        float fTemp0 = 0.0;
        // collect input for the meter widget
        float fTemp1 = (float)input0[i];
        float fRec3 = fmax(fConst0, fabsf(fTemp1));
        int iTemp1 = int((iRecb1[1] < 4096));
        fRecb0[0] = ((iTemp1)?fmax(fRecb0[1], fRec3):fRec3);
        iRecb1[0] = ((iTemp1)?(1 + iRecb1[1]):1);
        fRecb2[0] = ((iTemp1)?fRecb2[1]:fRecb0[1]);
        
        if (iSlow0) { //record
            // delay recording by measured rountrip latency
            if  (latency > roundtrip) {
                if (iA) {
                    fRec1[IOTA++] = fTemp1;
                } else {
                    fRec0[IOTA++] = fTemp1;
                }
                fConst1 = fmax(fConst1, fabsf(fTemp1));
            }
            if (IOTA > MAXRECSIZE-1) { // when buffer is full, flush to stream
                iA = iA ? 0 : 1 ;
                tape = iA ? fRec0 : fRec1;
                keep_stream = true;
                savesize = IOTA;
                sem_post(&m_trig);
                IOTA = 0;
            }
            // play input.wav file once
            if (IOTAP < inputsize) {
                fTemp0 = tape1[IOTAP];
                fConst2 = fmax(fConst2, fabsf(fTemp0));
                IOTAP++;
            }
            latency++;
            // switch of recording when record time match play time
            if (latency > (inputsize + roundtrip)) {
                finish = 1;
                IOTAP = 0;
                latency = 0;
                roundtrip = 0;
                measure = 0;
                iSlow0 = 0;
                // switch off the PROFILE button when host support it
                requestParameterValueChange((PortIndex)PROFILE, 0.0f);
                // check if we need normalization
                if (fConst1 > fConst2)
                    nf = fConst2 / fConst1;
                else
                    nf = 1.0;
                //fprintf(stderr, "finished max %f \n", nf);
            }
            
        } else if (IOTA) { // when record stoped, flush the rest to stream
            tape = iA ? fRec1 : fRec0;
            savesize = IOTA;
            keep_stream = false;
            sem_post(&m_trig);
            IOTA = 0;
            iA = 0;
            IOTAP = 0;
            latency = 0;
            roundtrip = 0;
            measure = 0;
        }
        output0[i] = fTemp0;
        // post processing
        fRecb2[1] = fRecb2[0];
        iRecb1[1] = iRecb1[0];
        fRecb0[1] = fRecb0[0];
    }
    // vu-meter
    *fbargraph = 20.*log10(fmax(0.0000003,fRecb2[0]));
    // progress bar
    *fbargraph1 = finish ? 1.0 : float(float(IOTAP) / float(inputsize));
}

// static wrapper to run the process
void Profil::mono_audio(int count, float *input0, float *output0, Profil *p) {
    (p)->compute(count, input0, output0);
}

// connect parameters with ports from the host
void Profil::connect(uint32_t port,void* data) {
    switch ((PortIndex)port)
    {
    case PROFILE: 
        fcheckbox0 = (float*)data; // , 0.0f, 0.0f, 1.0f, 1.0f 
        break;
    case CLIP: 
        fcheckbox1 = (float*)data; // , 0.0f, 0.0f, 1.0f, 1.0f 
        break;
    case METER: 
        fbargraph = (float*)data; // , -70.0, -70.0, 4.0, 0.00001 
        break;
    case STATE: 
        fbargraph1 = (float*)data; // , 0.0, 0.0, 1.0, 0.00001 
        break;
    case ERRORS: 
        errors = (float*)data; // , 0.0, 0.0, 4.0, 1.0f
        break;
    default:
        break;
    }
}

// static wrapper to call internal connect
void Profil::connect_ports(uint32_t port,void* data, Profil *p) {
    (p)->connect(port, data);
}

// delete the plug
void Profil::delete_instance(Profil *p) {
    delete (p);
}

} // end namespace profiler
