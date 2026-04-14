#pragma once

#ifdef ENABLE_SPECTRALIZER

#include <complex>

#define SPECTRALIZER_BACKEND_MKL 1
#define SPECTRALIZER_BACKEND_FFTW 2

template <class Impl>
class FFTBase : public Impl {
    double* input_;
    std::complex<double>* output_;

  public:
    FFTBase(const FFTBase&) = delete;
    FFTBase(FFTBase&&) = delete;
    FFTBase& operator=(const FFTBase&) = delete;
    FFTBase& operator=(FFTBase&&) = delete;

    FFTBase(unsigned len, double* input, std::complex<double>* output) :
        input_(input), output_(output) {
        Impl::createPlan(len, input, output);
    }

    ~FFTBase() {
        Impl::freePlan();
    }

    void exec() {
        Impl::exec(input_, output_);
    }

    void resize(unsigned len) {
        Impl::freePlan();
        Impl::createPlan(len, input_, output_);
    }
};

#if ENABLE_SPECTRALIZER == SPECTRALIZER_BACKEND_MKL
#include <mkl_dfti.h>
#include <stdexcept>

class Mkl {
  public:
    void exec(double* input, std::complex<double>* output) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        if (DftiComputeForward(desc_, input, output) != DFTI_NO_ERROR) {
            throw std::runtime_error("MKL DftiComputeForward failed");
        }
    }

    void createPlan(unsigned len, [[maybe_unused]] double* input,
        [[maybe_unused]] std::complex<double>* out) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        if (DftiCreateDescriptor(&desc_, DFTI_DOUBLE, DFTI_REAL, 1, len) !=
            DFTI_NO_ERROR) {
            throw std::runtime_error("MKL DftiCreateDescriptor failed");
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        if (DftiSetValue(desc_, DFTI_PLACEMENT, DFTI_NOT_INPLACE) !=
            DFTI_NO_ERROR) {
            throw std::runtime_error("MKL DftiSetValue failed");
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        if (DftiCommitDescriptor(desc_) != DFTI_NO_ERROR) {
            throw std::runtime_error("MKL DftiCommitDescriptor failed");
        }
    }

    void freePlan() {
        DftiFreeDescriptor(&desc_);
    }

  private:
    DFTI_DESCRIPTOR_HANDLE desc_ = nullptr;
};

using FFT = FFTBase<Mkl>;

#elif ENABLE_SPECTRALIZER == SPECTRALIZER_BACKEND_FFTW
#include <fftw3.h>

class Fftw {
    fftw_plan plan_;

  public:
    void exec([[maybe_unused]] double* input,
        [[maybe_unused]] std::complex<double>* output) {
        fftw_execute(plan_);
    }

    void createPlan(unsigned len, double* input, std::complex<double>* output) {
        plan_ = fftw_plan_dft_r2c_1d(
            len, input, reinterpret_cast<fftw_complex*>(output), FFTW_ESTIMATE);
    }

    void freePlan() {
        fftw_destroy_plan(plan_);
    }
};

using FFT = FFTBase<Fftw>;

#endif

#endif
