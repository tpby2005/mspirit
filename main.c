#include <pulse/pulseaudio.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <fftw3.h>
#include <raylib.h>
#include <time.h>

static pa_mainloop *mainloop;
static pa_mainloop_api *mainloop_api;
static pa_context *context;

Color rotating_color = GREEN;
float magnitude_multiplier = 0.005f;

enum
{
    CROSS,
    BAR,
    LINE
} mode = CROSS;

bool hide = false;

Color HSLtoRGB(float H, float S, float L)
{
    float C = (1 - fabs(2 * L - 1)) * S;
    float X = C * (1 - fabs(fmod(H / 60.0, 2) - 1));
    float m = L - C / 2;
    float Rs, Gs, Bs;

    if (H >= 0 && H < 60)
    {
        Rs = C;
        Gs = X;
        Bs = 0;
    }
    else if (H >= 60 && H < 120)
    {
        Rs = X;
        Gs = C;
        Bs = 0;
    }
    else if (H >= 120 && H < 180)
    {
        Rs = 0;
        Gs = C;
        Bs = X;
    }
    else if (H >= 180 && H < 240)
    {
        Rs = 0;
        Gs = X;
        Bs = C;
    }
    else if (H >= 240 && H < 300)
    {
        Rs = X;
        Gs = 0;
        Bs = C;
    }
    else
    {
        Rs = C;
        Gs = 0;
        Bs = X;
    }

    return (Color){(Rs + m) * 255, (Gs + m) * 255, (Bs + m) * 255, 255};
}

static void stream_read_callback(pa_stream *s, size_t length, void *userdata)
{
    const void *data;

    clock_t current_time = clock();
    static clock_t last_time = 0;
    static float hue = 0;

    double time_diff = ((double)(current_time - last_time) / CLOCKS_PER_SEC);

    if (time_diff >= 0.005)
    {
        rotating_color = HSLtoRGB(hue, 1.0, 0.5);
        hue = fmod(hue + 1, 360);
        last_time = current_time;
    }

    if (pa_stream_peek(s, &data, &length) < 0)
    {
        printf("pa_stream_peek() failed: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));

        return;
    }

    int16_t *samples = (int16_t *)data;
    size_t num_samples = length / sizeof(int16_t);

    if (IsKeyPressed(KEY_ONE))
    {
        mode = CROSS;
    }
    if (IsKeyPressed(KEY_TWO))
    {
        mode = BAR;
    }
    if (IsKeyPressed(KEY_THREE))
    {
        mode = LINE;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q))
    {
        exit(0);
    }

    if (num_samples > 1000)
    {
        fftw_complex *in = fftw_alloc_complex(num_samples);
        fftw_complex *out = fftw_alloc_complex(num_samples);

        for (size_t i = 0; i < num_samples; i++)
        {
            in[i][0] = samples[i];
            in[i][1] = 0;
        }

        fftw_plan plan = fftw_plan_dft_1d(num_samples, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

        fftw_execute(plan);

        BeginDrawing();
        ClearBackground(BLACK);

        int mouse_wheel = GetMouseWheelMove();
        magnitude_multiplier += mouse_wheel * 0.001f;

        if (magnitude_multiplier < 0.001f)
        {
            magnitude_multiplier = 0.001f;
        }

        if (magnitude_multiplier > 0.01f)
        {
            magnitude_multiplier = 0.01f;
        }

        size_t target_samples = 500;

        if (target_samples > num_samples)
        {
            target_samples = num_samples;
        }

        float *resampled_data = malloc(sizeof(float) * target_samples);

        for (size_t i = 0; i < target_samples; i++)
        {
            float magnitude = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
            resampled_data[i] = magnitude;
        }

        size_t smoothing_window = 7;

        float *smoothed_data = malloc(sizeof(float) * target_samples);

        for (size_t i = 0; i < smoothing_window && i < target_samples; i++)
        {
            smoothed_data[i] = resampled_data[i];
        }

        size_t num_frames = 4;

        static float **prev_frames_data = NULL;
        if (prev_frames_data == NULL)
        {
            prev_frames_data = malloc(num_frames * sizeof(float *));
            for (size_t i = 0; i < num_frames; i++)
            {
                prev_frames_data[i] = calloc(target_samples, sizeof(float));
            }
        }

        for (size_t i = 0; i < target_samples; i++)
        {
            float sum = 0;
            float weights_sum = 0;
            for (size_t j = 0; j < smoothing_window; j++)
            {
                float weight = 1.0;
                if (i >= j)
                {
                    float data = resampled_data[i - j];
                    sum += weight * data;
                    weights_sum += weight;
                }
                else
                {
                    for (size_t k = 0; k < num_frames; k++)
                    {
                        float weight = 1.0 + (k * 0.5);
                        sum += weight * prev_frames_data[k][smoothing_window - j - 1];
                        weights_sum += weight;
                    }
                }
            }
            smoothed_data[i] = sum / weights_sum;
        }

        float *temp = prev_frames_data[num_frames - 1];
        for (size_t i = num_frames - 1; i > 0; i--)
        {
            prev_frames_data[i] = prev_frames_data[i - 1];
        }
        prev_frames_data[0] = temp;

        memcpy(prev_frames_data[0], resampled_data, target_samples * sizeof(float));

        switch (mode)
        {
        case CROSS:
            for (size_t i = 0; i < target_samples / 2; i++)
            {
                float height = smoothed_data[i] * magnitude_multiplier;
                size_t draw_position_left = (size_t)((float)(target_samples / 2 - i) / target_samples * GetScreenWidth());
                size_t draw_position_right = (size_t)((float)(target_samples / 2 + i) / target_samples * GetScreenWidth());
                DrawRectangle(draw_position_left, (GetScreenHeight() / 2) - height, GetScreenWidth() > 1000 ? 4 : 2, height * 2, rotating_color);
                DrawRectangle(draw_position_right, (GetScreenHeight() / 2) - height, GetScreenWidth() > 1000 ? 4 : 2, height * 2, rotating_color);
            }
            break;

        case BAR:
            for (size_t i = 0; i < target_samples; i++)
            {
                float height = smoothed_data[i] * magnitude_multiplier;
                size_t draw_position = (size_t)((float)(target_samples - i) / target_samples * GetScreenWidth());
                DrawRectangle(draw_position, (GetScreenHeight()) - height, GetScreenWidth() > 1000 ? 4 : 2, height, rotating_color);
            }
            break;

        case LINE:
            for (size_t i = 0; i < target_samples - 1; i++)
            {
                float height = smoothed_data[i] * magnitude_multiplier;
                float next_height = smoothed_data[i + 1] * magnitude_multiplier;
                size_t draw_position = (size_t)((float)(target_samples - i) / target_samples * GetScreenWidth());
                size_t next_draw_position = (size_t)((float)(target_samples - i - 2) / target_samples * GetScreenWidth());

                DrawLine(draw_position, (GetScreenHeight()) - height, next_draw_position, (GetScreenHeight()) - next_height, rotating_color);
            }
            break;
        }

        free(resampled_data);
        free(smoothed_data);

        int thousandths_place = ((int)(magnitude_multiplier * 1000)) % 10;

        if (IsKeyPressed(KEY_H))
        {
            hide = !hide;
        }

        if (!hide)
        {
            if (magnitude_multiplier == 0.01f)
            {
                DrawText("Magnitude: 10", 10, 10, 20, WHITE);
            }
            else
            {
                DrawText(TextFormat("Magnitude: %i", thousandths_place), 10, 10, 20, WHITE);
            }
        }

        EndDrawing();

        fftw_destroy_plan(plan);
        fftw_free(in);
        fftw_free(out);
    }

    pa_stream_drop(s);
}

static void stream_state_callback(pa_stream *s, void *userdata)
{
    switch (pa_stream_get_state(s))
    {
    case PA_STREAM_READY:
        break;

    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
        pa_mainloop_quit(mainloop, 1);
        break;

    default:
        break;
    }
}

char monitor_name[256];

static void context_state_callback(pa_context *c, void *userdata)
{
    switch (pa_context_get_state(c))
    {
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
        break;

    case PA_CONTEXT_READY:
    {
        pa_sample_spec sample_spec = {
            .format = PA_SAMPLE_S16LE,
            .rate = 48000,
            .channels = 2};

        pa_stream *stream = pa_stream_new(c, "mspirit", &sample_spec, NULL);
        pa_stream_set_read_callback(stream, stream_read_callback, NULL);
        pa_stream_set_state_callback(stream, stream_state_callback, NULL);
        pa_stream_connect_record(stream, monitor_name, NULL, PA_STREAM_NOFLAGS);
        break;
    }

    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        pa_mainloop_quit(mainloop, 1);
        break;

    default:
        break;
    }
}

void usage()
{
    printf("Usage: mspirit (monitor name)\n");
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc != 2)
        usage();

    strcpy(monitor_name, argv[1]);

    mainloop = pa_mainloop_new();
    mainloop_api = pa_mainloop_get_api(mainloop);
    context = pa_context_new(mainloop_api, "mspirit");

    pa_context_set_state_callback(context, context_state_callback, NULL);
    pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(800, 600, "mspirit");

    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();

    pa_mainloop_run(mainloop, NULL);

    pa_context_unref(context);
    pa_mainloop_free(mainloop);

    return 0;
}