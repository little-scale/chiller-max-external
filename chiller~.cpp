#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "ext_buffer.h"
#include "ext_systime.h"
#include <complex>
#include <cmath>
#include <vector>
#include <random>

static t_class *chiller_class;

#define CHILLER_DEFAULT_FFT_SIZE 2048

typedef struct _chiller {
    t_pxobject ob;
    
    // Buffer reference
    t_buffer_ref *buffer_ref;
    t_symbol *buffer_name;
    
    // Analysis and synthesis
    std::vector<std::complex<double>> *frozen_spectrum;
    std::vector<double> *window;
    std::vector<double> *overlap_buffer_l;
    std::vector<double> *overlap_buffer_r;
    
    // FFT workspace
    std::vector<std::complex<double>> *fft_buffer;
    std::vector<double> *analysis_buffer;
    
    // Parameters
    long fft_size;             // FFT size (configurable at instantiation)
    long hop_size;             // Hop size (fft_size / 4)
    double position;           // 0.0 to 1.0 - position in buffer to freeze
    double overlap_amount;     // overlap factor for grain synthesis
    double grain_rate;         // rate of grain generation
    double phase_randomness;   // amount of phase randomization
    double amplitude_variation; // amplitude variation amount
    
    // State
    bool spectrum_captured;
    bool capturing_spectrum;  // Flag to prevent concurrent captures
    long grain_counter;
    long hop_counter;
    double sample_rate;
    double last_position_change_time;  // Time of last position change
    
    // Random number generation
    std::mt19937 *rng;
    std::uniform_real_distribution<double> *phase_dist;
    std::uniform_real_distribution<double> *amp_dist;
    
} t_chiller;

// Function prototypes
void *chiller_new(t_symbol *s, long argc, t_atom *argv);
void chiller_free(t_chiller *x);
void chiller_dsp64(t_chiller *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void chiller_perform64(t_chiller *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void chiller_assist(t_chiller *x, void *b, long m, long a, char *s);

// Message handlers
void chiller_set_buffer(t_chiller *x, t_symbol *s);
void chiller_set_position(t_chiller *x, double pos);
void chiller_set_overlap(t_chiller *x, double overlap);
void chiller_set_rate(t_chiller *x, double rate);
void chiller_set_phase_rand(t_chiller *x, double rand_amount);
void chiller_set_amp_var(t_chiller *x, double var_amount);
void chiller_freeze(t_chiller *x);
void chiller_debug(t_chiller *x);
void chiller_notify(t_chiller *x, t_symbol *s, t_symbol *msg, void *sender, void *data);

// Utility functions
void chiller_capture_spectrum(t_chiller *x);
void chiller_apply_window(std::vector<double>& buffer, const std::vector<double>& window);
void chiller_fft(std::vector<std::complex<double>>& data);
void chiller_ifft(std::vector<std::complex<double>>& data);
void chiller_generate_window(std::vector<double>& window, long size);

void ext_main(void *r) {
    t_class *c = class_new("chiller~", (method)chiller_new, (method)chiller_free, sizeof(t_chiller), NULL, A_GIMME, 0);
    
    class_addmethod(c, (method)chiller_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)chiller_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)chiller_set_buffer, "set", A_SYM, 0);
    class_addmethod(c, (method)chiller_set_position, "position", A_FLOAT, 0);
    class_addmethod(c, (method)chiller_set_overlap, "overlap", A_FLOAT, 0);
    class_addmethod(c, (method)chiller_set_rate, "rate", A_FLOAT, 0);
    class_addmethod(c, (method)chiller_set_phase_rand, "phaserand", A_FLOAT, 0);
    class_addmethod(c, (method)chiller_set_amp_var, "ampvar", A_FLOAT, 0);
    class_addmethod(c, (method)chiller_freeze, "freeze", 0);
    class_addmethod(c, (method)chiller_debug, "bang", 0);
    class_addmethod(c, (method)chiller_notify, "notify", A_CANT, 0);
    
    class_dspinit(c);
    class_register(CLASS_BOX, c);
    chiller_class = c;
}

void *chiller_new(t_symbol *s, long argc, t_atom *argv) {
    t_chiller *x = (t_chiller *)object_alloc(chiller_class);
    
    if (x) {
        dsp_setup((t_pxobject *)x, 0);
        outlet_new(x, "signal");
        outlet_new(x, "signal");
        
        // Parse FFT size argument (default 2048, must be power of 2)
        x->fft_size = CHILLER_DEFAULT_FFT_SIZE;
        if (argc > 0 && atom_gettype(argv) == A_LONG) {
            long requested_size = atom_getlong(argv);
            // Validate that it's a power of 2 and reasonable size
            if (requested_size >= 512 && requested_size <= 8192 && 
                (requested_size & (requested_size - 1)) == 0) {
                x->fft_size = requested_size;
            } else {
                object_error((t_object *)x, "FFT size must be power of 2 between 512 and 8192, using default %ld", CHILLER_DEFAULT_FFT_SIZE);
            }
        }
        
        x->hop_size = x->fft_size / 4;  // Hop size is 1/4 of FFT size
        
        // Initialize C++ objects with dynamic size
        x->frozen_spectrum = new std::vector<std::complex<double>>(x->fft_size);
        x->window = new std::vector<double>(x->fft_size);
        x->overlap_buffer_l = new std::vector<double>(x->fft_size, 0.0);
        x->overlap_buffer_r = new std::vector<double>(x->fft_size, 0.0);
        x->fft_buffer = new std::vector<std::complex<double>>(x->fft_size);
        x->analysis_buffer = new std::vector<double>(x->fft_size, 0.0);
        
        x->rng = new std::mt19937(std::random_device{}());
        x->phase_dist = new std::uniform_real_distribution<double>(-M_PI, M_PI);
        x->amp_dist = new std::uniform_real_distribution<double>(-1.0, 1.0);
        
        // Initialize parameters
        x->position = 0.5;
        x->overlap_amount = 4.0;
        x->grain_rate = 1.0;
        x->phase_randomness = 0.1;
        x->amplitude_variation = 0.1;
        
        // Initialize state
        x->spectrum_captured = false;
        x->capturing_spectrum = false;
        x->grain_counter = 0;
        x->hop_counter = 0;
        x->sample_rate = 44100.0;
        x->last_position_change_time = 0.0;
        
        // Generate Hann window
        chiller_generate_window(*x->window, x->fft_size);
        
        // Initialize buffer reference
        x->buffer_ref = NULL;
        x->buffer_name = gensym("");
        
        // Process arguments - buffer name can be second argument
        if (argc > 1 && atom_gettype(argv + 1) == A_SYM) {
            chiller_set_buffer(x, atom_getsym(argv + 1));
        } else if (argc > 0 && atom_gettype(argv) == A_SYM) {
            // If first arg is symbol and no second arg, treat as buffer name
            chiller_set_buffer(x, atom_getsym(argv));
        }
        
        object_post((t_object *)x, "chiller~ initialized with FFT size %ld", x->fft_size);
    }
    
    return x;
}

void chiller_free(t_chiller *x) {
    dsp_free((t_pxobject *)x);
    
    if (x->buffer_ref) {
        object_free(x->buffer_ref);
    }
    
    delete x->frozen_spectrum;
    delete x->window;
    delete x->overlap_buffer_l;
    delete x->overlap_buffer_r;
    delete x->fft_buffer;
    delete x->analysis_buffer;
    delete x->rng;
    delete x->phase_dist;
    delete x->amp_dist;
}

void chiller_dsp64(t_chiller *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags) {
    x->sample_rate = samplerate;
    object_method(dsp64, gensym("dsp_add64"), x, chiller_perform64, 0, NULL);
}

void chiller_perform64(t_chiller *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam) {
    double *out_l = outs[0];
    double *out_r = outs[1];
    
    if (!x->spectrum_captured || !x->buffer_ref) {
        // Output silence if no spectrum captured or no buffer
        for (long i = 0; i < sampleframes; i++) {
            out_l[i] = 0.0;
            out_r[i] = 0.0;
        }
        return;
    }
    
    for (long i = 0; i < sampleframes; i++) {
        x->hop_counter++;
        
        // Generate new grain when hop counter reaches hop size
        if (x->hop_counter >= x->hop_size / x->grain_rate) {
            x->hop_counter = 0;
            
            // Copy frozen spectrum and apply phase randomization
            for (size_t j = 0; j < x->frozen_spectrum->size(); j++) {
                double magnitude = std::abs((*x->frozen_spectrum)[j]);
                double phase = std::arg((*x->frozen_spectrum)[j]);
                
                // Add phase randomization
                phase += (*x->phase_dist)(*x->rng) * x->phase_randomness;
                
                // Apply amplitude variation
                magnitude *= 1.0 + (*x->amp_dist)(*x->rng) * x->amplitude_variation;
                
                (*x->fft_buffer)[j] = std::polar(magnitude, phase);
            }
            
            // Inverse FFT
            chiller_ifft(*x->fft_buffer);
            
            // Apply window and overlap-add to buffers
            for (size_t j = 0; j < x->fft_buffer->size(); j++) {
                double sample = x->fft_buffer->at(j).real() * x->window->at(j);
                
                // Add to overlap buffers with stereo spread
                (*x->overlap_buffer_l)[j] += sample * 0.8;  // Slight left bias
                (*x->overlap_buffer_r)[j] += sample * 1.0;  // Slight right bias
            }
        }
        
        // Output samples and shift overlap buffers
        out_l[i] = (*x->overlap_buffer_l)[0] * 0.1;  // Scale down output
        out_r[i] = (*x->overlap_buffer_r)[0] * 0.1;
        
        // Shift overlap buffers
        for (size_t j = 0; j < x->overlap_buffer_l->size() - 1; j++) {
            (*x->overlap_buffer_l)[j] = (*x->overlap_buffer_l)[j + 1];
            (*x->overlap_buffer_r)[j] = (*x->overlap_buffer_r)[j + 1];
        }
        (*x->overlap_buffer_l)[x->overlap_buffer_l->size() - 1] = 0.0;
        (*x->overlap_buffer_r)[x->overlap_buffer_r->size() - 1] = 0.0;
    }
}

void chiller_assist(t_chiller *x, void *b, long m, long a, char *s) {
    if (m == ASSIST_INLET) {
        snprintf(s, 256, "Commands: set <buffer>, position <0-1>, freeze");
    } else {
        switch (a) {
            case 0: snprintf(s, 256, "(signal) Left output"); break;
            case 1: snprintf(s, 256, "(signal) Right output"); break;
        }
    }
}

void chiller_set_buffer(t_chiller *x, t_symbol *s) {
    if (x->buffer_ref) {
        object_free(x->buffer_ref);
    }
    
    x->buffer_name = s;
    x->buffer_ref = buffer_ref_new((t_object *)x, s);
    x->spectrum_captured = false;
}

void chiller_set_position(t_chiller *x, double pos) {
    double current_time = systimer_gettime();
    double min_interval = 500.0;  // 500ms minimum between position changes
    
    // Check if enough time has passed since last position change
    if (current_time - x->last_position_change_time < min_interval) {
        // Ignore this position change - too soon
        return;
    }
    
    x->position = CLAMP(pos, 0.0, 1.0);
    x->last_position_change_time = current_time;
    
    // Only capture if not already capturing to prevent rapid-fire position changes
    if (!x->capturing_spectrum) {
        // Temporarily stop audio output during capture
        x->spectrum_captured = false;
        chiller_capture_spectrum(x);
    }
}

void chiller_set_overlap(t_chiller *x, double overlap) {
    x->overlap_amount = CLAMP(overlap, 1.0, 8.0);
}

void chiller_set_rate(t_chiller *x, double rate) {
    x->grain_rate = CLAMP(rate, 0.1, 4.0);
}

void chiller_set_phase_rand(t_chiller *x, double rand_amount) {
    x->phase_randomness = CLAMP(rand_amount, 0.0, 1.0);
}

void chiller_set_amp_var(t_chiller *x, double var_amount) {
    x->amplitude_variation = CLAMP(var_amount, 0.0, 0.5);
}

void chiller_freeze(t_chiller *x) {
    chiller_capture_spectrum(x);
}

void chiller_debug(t_chiller *x) {
    object_post((t_object *)x, "=== CHILLER DEBUG INFO ===");
    
    // Basic configuration
    object_post((t_object *)x, "FFT Size: %ld, Hop Size: %ld", x->fft_size, x->hop_size);
    object_post((t_object *)x, "Sample Rate: %.1f Hz", x->sample_rate);
    
    // Buffer info
    if (x->buffer_ref) {
        t_buffer_obj *buffer = buffer_ref_getobject(x->buffer_ref);
        if (buffer) {
            long frames = buffer_getframecount(buffer);
            long channels = buffer_getchannelcount(buffer);
            object_post((t_object *)x, "Buffer: %s (%ld frames, %ld channels)", x->buffer_name->s_name, frames, channels);
        } else {
            object_post((t_object *)x, "Buffer: %s (NOT FOUND)", x->buffer_name->s_name);
        }
    } else {
        object_post((t_object *)x, "Buffer: NONE SET");
    }
    
    // Analysis state
    object_post((t_object *)x, "Position: %.3f", x->position);
    object_post((t_object *)x, "Spectrum Captured: %s", x->spectrum_captured ? "YES" : "NO");
    object_post((t_object *)x, "Currently Capturing: %s", x->capturing_spectrum ? "YES" : "NO");
    
    // Timing info
    double current_time = systimer_gettime();
    double time_since_change = current_time - x->last_position_change_time;
    object_post((t_object *)x, "Time since last position change: %.1f ms", time_since_change);
    
    // Synthesis parameters
    object_post((t_object *)x, "Grain Rate: %.2f", x->grain_rate);
    object_post((t_object *)x, "Phase Randomness: %.2f", x->phase_randomness);
    object_post((t_object *)x, "Amplitude Variation: %.2f", x->amplitude_variation);
    object_post((t_object *)x, "Overlap Amount: %.2f", x->overlap_amount);
    
    // Real-time state
    object_post((t_object *)x, "Hop Counter: %ld (next grain at %ld)", x->hop_counter, (long)(x->hop_size / x->grain_rate));
    object_post((t_object *)x, "Grain Counter: %ld", x->grain_counter);
    
    // Spectrum analysis (if captured)
    if (x->spectrum_captured && x->frozen_spectrum) {
        double spectrum_energy = 0.0;
        double max_magnitude = 0.0;
        int nonzero_bins = 0;
        
        for (size_t i = 0; i < x->frozen_spectrum->size(); i++) {
            double mag = std::abs((*x->frozen_spectrum)[i]);
            spectrum_energy += mag * mag;
            if (mag > max_magnitude) max_magnitude = mag;
            if (mag > 1e-6) nonzero_bins++;
        }
        
        object_post((t_object *)x, "Spectrum Energy: %.6f", spectrum_energy);
        object_post((t_object *)x, "Max Magnitude: %.6f", max_magnitude);
        object_post((t_object *)x, "Non-zero bins: %d/%ld", nonzero_bins, x->frozen_spectrum->size());
        
        // Target energy for comparison
        double target_energy = x->fft_size * 0.1;
        object_post((t_object *)x, "Target Energy: %.6f (normalization %s)", 
                   target_energy, (spectrum_energy > target_energy) ? "ACTIVE" : "inactive");
    }
    
    // Overlap buffer analysis
    if (x->overlap_buffer_l && x->overlap_buffer_r) {
        double buffer_energy_l = 0.0;
        double buffer_energy_r = 0.0;
        double max_val_l = 0.0;
        double max_val_r = 0.0;
        
        for (size_t i = 0; i < x->overlap_buffer_l->size(); i++) {
            double val_l = std::abs((*x->overlap_buffer_l)[i]);
            double val_r = std::abs((*x->overlap_buffer_r)[i]);
            buffer_energy_l += val_l * val_l;
            buffer_energy_r += val_r * val_r;
            if (val_l > max_val_l) max_val_l = val_l;
            if (val_r > max_val_r) max_val_r = val_r;
        }
        
        object_post((t_object *)x, "Overlap Buffer L - Energy: %.6f, Max: %.6f", buffer_energy_l, max_val_l);
        object_post((t_object *)x, "Overlap Buffer R - Energy: %.6f, Max: %.6f", buffer_energy_r, max_val_r);
        
        // Show first few samples for debugging
        object_post((t_object *)x, "Buffer head L: [%.4f, %.4f, %.4f, %.4f]", 
                   (*x->overlap_buffer_l)[0], (*x->overlap_buffer_l)[1], 
                   (*x->overlap_buffer_l)[2], (*x->overlap_buffer_l)[3]);
    }
    
    object_post((t_object *)x, "=== END DEBUG INFO ===");
}

void chiller_notify(t_chiller *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
    if (msg == gensym("globalsymbol_binding")) {
        // Buffer binding changed
        x->spectrum_captured = false;
    }
}

void chiller_capture_spectrum(t_chiller *x) {
    if (!x->buffer_ref) {
        object_error((t_object *)x, "No buffer set");
        return;
    }
    
    // Set capturing flag to prevent concurrent captures
    x->capturing_spectrum = true;
    
    t_buffer_obj *buffer = buffer_ref_getobject(x->buffer_ref);
    if (!buffer) {
        object_error((t_object *)x, "Buffer not found");
        x->capturing_spectrum = false;
        return;
    }
    
    float *buffer_samples = buffer_locksamples(buffer);
    if (!buffer_samples) {
        object_error((t_object *)x, "Could not access buffer data");
        x->capturing_spectrum = false;
        return;
    }
    
    long buffer_frames = buffer_getframecount(buffer);
    long buffer_channels = buffer_getchannelcount(buffer);
    
    if (buffer_frames < x->fft_size) {
        buffer_unlocksamples(buffer);
        object_error((t_object *)x, "Buffer too small (need at least %ld samples)", x->fft_size);
        x->capturing_spectrum = false;
        return;
    }
    
    // Calculate starting position in buffer
    long start_frame = (long)(x->position * (buffer_frames - x->fft_size));
    
    // Copy samples to analysis buffer
    for (long i = 0; i < x->fft_size; i++) {
        if (buffer_channels == 1) {
            (*x->analysis_buffer)[i] = buffer_samples[start_frame + i];
        } else {
            // Mix stereo to mono
            (*x->analysis_buffer)[i] = (buffer_samples[(start_frame + i) * 2] + 
                                       buffer_samples[(start_frame + i) * 2 + 1]) * 0.5;
        }
    }
    
    // Apply window
    chiller_apply_window(*x->analysis_buffer, *x->window);
    
    // Copy to FFT buffer
    for (size_t i = 0; i < x->analysis_buffer->size(); i++) {
        (*x->fft_buffer)[i] = std::complex<double>((*x->analysis_buffer)[i], 0.0);
    }
    
    // Perform FFT
    chiller_fft(*x->fft_buffer);
    
    // Calculate spectrum energy for normalization
    double spectrum_energy = 0.0;
    for (size_t i = 0; i < x->fft_buffer->size(); i++) {
        double magnitude = std::abs((*x->fft_buffer)[i]);
        spectrum_energy += magnitude * magnitude;
    }
    
    // Normalize spectrum to prevent magnitude explosion
    // Target energy level based on FFT size (prevents feedback loops)
    double target_energy = x->fft_size * 0.1;  // Reasonable energy level
    if (spectrum_energy > 1e-10) {  // Avoid division by zero
        double normalization_factor = sqrt(target_energy / spectrum_energy);
        
        // Apply normalization
        for (size_t i = 0; i < x->fft_buffer->size(); i++) {
            (*x->fft_buffer)[i] *= normalization_factor;
        }
    }
    
    // Store frozen spectrum
    *x->frozen_spectrum = *x->fft_buffer;
    
    // Clear overlap buffers to prevent noise artifacts
    std::fill(x->overlap_buffer_l->begin(), x->overlap_buffer_l->end(), 0.0);
    std::fill(x->overlap_buffer_r->begin(), x->overlap_buffer_r->end(), 0.0);
    
    // Reset hop counter to start fresh grain generation
    x->hop_counter = 0;
    
    x->spectrum_captured = true;
    x->capturing_spectrum = false;
    
    // Unlock buffer samples
    buffer_unlocksamples(buffer);
    
    object_post((t_object *)x, "Spectrum captured at position %.3f", x->position);
}

void chiller_apply_window(std::vector<double>& buffer, const std::vector<double>& window) {
    for (size_t i = 0; i < buffer.size() && i < window.size(); i++) {
        buffer[i] *= window[i];
    }
}

void chiller_fft(std::vector<std::complex<double>>& data) {
    // Simple radix-2 Cooley-Tukey FFT implementation
    long n = data.size();
    if (n <= 1) return;
    
    // Bit-reverse reordering
    for (long i = 1, j = 0; i < n; i++) {
        long bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }
    
    // FFT computation
    for (long len = 2; len <= n; len <<= 1) {
        double ang = 2 * M_PI / len;
        std::complex<double> wlen(cos(ang), sin(ang));
        for (long i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (long j = 0; j < len / 2; j++) {
                std::complex<double> u = data[i + j];
                std::complex<double> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

void chiller_ifft(std::vector<std::complex<double>>& data) {
    // Conjugate
    for (auto& x : data) {
        x = std::conj(x);
    }
    
    // Forward FFT
    chiller_fft(data);
    
    // Conjugate and scale
    for (auto& x : data) {
        x = std::conj(x) / (double)data.size();
    }
}

void chiller_generate_window(std::vector<double>& window, long size) {
    for (long i = 0; i < size; i++) {
        window[i] = 0.5 * (1.0 - cos(2.0 * M_PI * i / (size - 1)));  // Hann window
    }
}