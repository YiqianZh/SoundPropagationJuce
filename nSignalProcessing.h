// Signal Processing - simple DSP related structs and functions
// Author - Nic Taylor
#pragma once

namespace nDSP
{
    struct Butterworth1Pole
    {
        float a1;
        float b0;
        float b1;

        float x1;
        float y1;

        bool bypass;

        Butterworth1Pole() :
            x1(0.0),
            y1(0.0),
            bypass(false) {}

        void Initialize(float cutoff_frequency, float sample_rate);

        float process(float x)
        {
            if (!bypass)
            {
                y1 = b0*x + b1*x1 - a1*y1;
                x1 = x;

                return y1;
            }
            return x;
        }
    };
}
