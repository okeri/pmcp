#pragma once

#ifdef ENABLE_SPECTRALIZER

#include <complex>

#define MKL 1
#define FFTW 2

template <class Impl>
class FFTBase : public Impl {
    double* in_;
    std::complex<double>* out_;

  public:
    FFTBase(const FFTBase&) = delete;
    FFTBase(FFTBase&&) = delete;
    FFTBase& operator=(const FFTBase&) = delete;
    FFTBase& operator=(FFTBase&&) = delete;

    FFTBase(unsigned len, double* in, std::complex<double>* out) :
        in_(in), out_(out) {
        Impl::createPlan(len, in, out);
    }

    ~FFTBase() {
        Impl::freePlan();
    }

    void exec() {
        Impl::exec(in_, out_);
    }

    void resize(unsigned len) {
        Impl::freePlan();
        Impl::createPlan(len, in_, out_);
    }
};

#if ENABLE_SPECTRALIZER == MKL
#include <mkl_dfti.h>

class Mkl {
  public:
    void exec(double* in, std::complex<double>* out) {
        DftiComputeForward(desc_, in, out);
    }

    void createPlan(unsigned len, [[maybe_unused]] double* in,
        [[maybe_unused]] std::complex<double>* out) {
        DftiCreateDescriptor(&desc_, DFTI_DOUBLE, DFTI_REAL, 1, len);
        DftiSetValue(desc_, DFTI_PLACEMENT, DFTI_NOT_INPLACE);
        DftiCommitDescriptor(desc_);
    }

    void freePlan() {
        DftiFreeDescriptor(&desc_);
    }

  private:
    DFTI_DESCRIPTOR_HANDLE desc_ = nullptr;
};

using FFT = FFTBase<Mkl>;

#elif ENABLE_SPECTRALIZER == FFTW
#include <fftw3.h>

class Fftw {
    fftw_plan plan_;

  public:
    void exec([[maybe_unused]] double* in,
        [[maybe_unused]] std::complex<double>* out) {
        fftw_execute(plan_);
    }

    void createPlan(unsigned len, double* in, std::complex<double>* out) {
        plan_ = fftw_plan_dft_r2c_1d(
            len, in, reinterpret_cast<fftw_complex*>(out), FFTW_ESTIMATE);
    }

    void freePlan() {
        fftw_destroy_plan(plan_);
    }
};

using FFT = FFTBase<Fftw>;

#endif

#endif
