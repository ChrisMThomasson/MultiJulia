/*
    Multi-Julia 2-ary Buddha Plotter
    By: Chris M. Thomasson
    Version: Pre-Alpha (0.0.0)


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*_____________________________________________________________*/


#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <complex>


#define CT_WIDTH (1920 * 1)
#define CT_HEIGHT (1080 * 1)
#define CT_N 10000000 // 26000000



typedef std::complex<double> ct_complex;
//#define I (ct_complex { 0, 1 })


struct ct_rgb_meta
{
    double red;
    double blue;
    double green;
    double alpha;
};


#define CT_RGB_META(mp_red, ct_blue, ct_green, ct_alpha) \
    { (mp_red), (ct_blue), (ct_green), (ct_alpha) }



struct ct_rgb
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    struct ct_rgb_meta meta; // meta data for this color
};


#define CT_RGB(mp_red, mp_green, mp_blue) \
    { (mp_red), (mp_green), (mp_blue), CT_RGB_META(0., 0., 0., 0.) }


struct ct_canvas
{
    unsigned long width;
    unsigned long height;
    struct ct_rgb* buf;
};

bool
ct_canvas_create(
    struct ct_canvas* const self,
    unsigned long width,
    unsigned long height
) {
    size_t size = width * height * sizeof(*self->buf);

    self->buf = (ct_rgb*)calloc(1, size);

    if (self->buf)
    {
        self->width = width;
        self->height = height;

        return true;
    }

    return false;
}

void
ct_canvas_destroy(
    struct ct_canvas const* const self
) {
    free(self->buf);
}



double
ct_canvas_log_density_post_processing_get_largest(
    struct ct_canvas const* const self
) {
    double largest = 0;

    size_t size = self->width * self->height;

    for (size_t i = 0; i < size; ++i)
    {
        struct ct_rgb const* c = self->buf + i;

        if (largest < c->meta.alpha)
        {
            largest = c->meta.alpha;
        }
    }

    return largest;
}


void
ct_canvas_log_density_post_processing(
    struct ct_canvas* const self
) {
    double largest = ct_canvas_log_density_post_processing_get_largest(self);
    size_t size = self->width * self->height;

    printf("largest = %f\n", largest);

    for (size_t i = 0; i < size; ++i)
    {
        struct ct_rgb* color = self->buf + i;

        if (color->meta.alpha > 0)
        {
            struct ct_rgb_meta c = color->meta;

            double dense = log10(c.alpha) / c.alpha;

            c.red *= dense;
            if (c.red > 1.0) c.red = 1.0;

            c.green *= dense;
            if (c.green > 1.0) c.green = 1.0;

            c.blue *= dense;
            if (c.blue > 1.0) c.blue = 1.0;

            color->r = c.red * 255;
            color->g = c.green * 255;
            color->b = c.blue * 255;
        }
    }
}




bool
ct_canvas_save_ppm(
    struct ct_canvas const* const self,
    char const* fname
) {
    FILE* fout = fopen(fname, "wb");

    if (fout)
    {
        char const ppm_head[] =
            "P6\n"
            "# Chris M. Thomasson Simple 2d Plane ver:0.0.0.0 (pre-alpha)";

        fprintf(fout, "%s\n%lu %lu\n%u\n",
            ppm_head,
            self->width, self->height,
            255U);

        size_t size = self->width * self->height;

        for (size_t i = 0; i < size; ++i)
        {
            struct ct_rgb* c = self->buf + i;
            fprintf(fout, "%c%c%c", c->r, c->g, c->b);
        }

        if (!fclose(fout))
        {
            return true;
        }
    }

    return false;
}




struct ct_axes
{
    double xmin;
    double xmax;
    double ymin;
    double ymax;
};

struct ct_axes
ct_axes_from_point(
    ct_complex z,
    double radius
) {
    struct ct_axes axes = {
        z.real() - radius, z.real() + radius,
        z.imag() - radius, z.imag() + radius
    };

    return axes;
}


struct ct_plane
{
    struct ct_axes axes;
    double xstep;
    double ystep;
};


void
ct_plane_init(
    struct ct_plane* const self,
    struct ct_axes const* axes,
    unsigned long width,
    unsigned long height
) {
    self->axes = *axes;

    double awidth = self->axes.xmax - self->axes.xmin;
    double aheight = self->axes.ymax - self->axes.ymin;

    assert(width > 0 && height > 0 && awidth > 0.0);

    double daspect = fabs((double)height / width);
    double waspect = fabs(aheight / awidth);

    if (daspect > waspect)
    {
        double excess = aheight * (daspect / waspect - 1.0);
        self->axes.ymax += excess / 2.0;
        self->axes.ymin -= excess / 2.0;
    }

    else if (daspect < waspect)
    {
        double excess = awidth * (waspect / daspect - 1.0);
        self->axes.xmax += excess / 2.0;
        self->axes.xmin -= excess / 2.0;
    }

    self->xstep = (self->axes.xmax - self->axes.xmin) / width;
    self->ystep = (self->axes.ymax - self->axes.ymin) / height;
}



struct ct_plot
{
    struct ct_plane plane;
    struct ct_canvas* canvas;
};


void
ct_plot_init(
    struct ct_plot* const self,
    struct ct_axes const* axes,
    struct ct_canvas* canvas
) {
    ct_plane_init(&self->plane, axes, canvas->width - 1, canvas->height - 1);
    self->canvas = canvas;
}


bool
ct_plot_addx(
    struct ct_plot* const self,
    ct_complex z,
    struct ct_rgb const* color
) {
    long x = (z.real() - self->plane.axes.xmin) / self->plane.xstep;
    long y = (self->plane.axes.ymax - z.imag()) / self->plane.ystep;

    if (x > -1 && x < (long)self->canvas->width &&
        y > -1 && y < (long)self->canvas->height)
    {
        // Now, we can convert to index.
        size_t i = x + y * self->canvas->width;

        assert(i < self->canvas->height * self->canvas->width);

        struct ct_rgb exist = self->canvas->buf[i];

        unsigned long addend = 3;

        // Sorry for the if ladder! Need to refine this... Yikes!
        if (exist.r < 255 - addend)
        {
            exist.r += addend;
        }

        else
        {
            exist.r = 255;

            if (exist.g < 255 - addend)
            {
                exist.g += addend;
            }

            else
            {
                exist.g = 255;

                if (exist.b < 255 - addend)
                {
                    exist.b += addend;
                }

                else
                {
                    exist.b = 255;
                }
            }
        }

        self->canvas->buf[i] = exist;
        return true;
    }

    return true;
}



bool
ct_plot_point(
    struct ct_plot* const self,
    ct_complex z,
    struct ct_rgb const* color
) {
    long x = (z.real() - self->plane.axes.xmin) / self->plane.xstep;
    long y = (self->plane.axes.ymax - z.imag()) / self->plane.ystep;

    if (x > -1 && x < (long)self->canvas->width &&
        y > -1 && y < (long)self->canvas->height)
    {
        // Now, we can convert to index.
        size_t i = x + y * self->canvas->width;

        assert(i < self->canvas->height * self->canvas->width);

        self->canvas->buf[i] = *color;
        return true;
    }

    return false;
}



bool
ct_plot_add(
    struct ct_plot* const self,
    ct_complex z,
    struct ct_rgb const* color
) {
    long x = (z.real() - self->plane.axes.xmin) / self->plane.xstep;
    long y = (self->plane.axes.ymax - z.imag()) / self->plane.ystep;

    if (x > -1 && x < (long)self->canvas->width &&
        y > -1 && y < (long)self->canvas->height)
    {
        // Now, we can convert to index.
        size_t i = x + y * self->canvas->width;

        assert(i < self->canvas->height * self->canvas->width);

        struct ct_rgb* exist = &self->canvas->buf[i];

        exist->meta.red += color->meta.red;
        exist->meta.green += color->meta.green;
        exist->meta.blue += color->meta.blue;
        exist->meta.alpha += 1.0;

        return true;
    }

    return true;
}



// The All Plotting Buddha
ct_complex
ct_all_plotting_buddha(
    struct ct_plot* const plot,
    ct_complex z,
    ct_complex c,
    unsigned long n
) {
    //double complex origin_z = z;

    struct ct_rgb color = CT_RGB(0, 0, 0);

    for (unsigned long i = 0; i < n; ++i)
    {
        z = z * z + c; // Mbrot formula

        if (i == 0)
        {
            color.meta.red = (0.681 + color.meta.red) / 2.0;
            color.meta.blue = (0.581 + color.meta.blue) / 2.0;
        }

        else if (i == 1)
        {
            color.meta.green = (0.681 + color.meta.green) / 2.0;
            color.meta.blue = (0.781 + color.meta.blue) / 2.0;
        }

        else
        {
            color.meta.blue = (0.681 + color.meta.blue) / 2.0;
        }

        ct_plot_add(plot, z, &color);
    }

    return z;
}


// Compute the fractal
void
ct_ifs(
    struct ct_plot* const plot,
    unsigned long n
) {
    // 3 Julia sets...
    ct_complex jp[] = {
        { 1, 1 },
        { -1, -1 },
        { 0, 2 }
    };

    ct_complex z = { 0, 0 };
    ct_complex c = { 0, 0 };

    struct ct_rgb color = CT_RGB(0, 0, 0);

    // Build the fractal...
    for (unsigned long i = 0; i < n; ++i)
    {
        double rn0 = rand() / (RAND_MAX - .0);
        double rn1 = rand() / (RAND_MAX - .0);

        // thirds...
        if (rn0 < 1. / 3)
        {
            c = jp[0];

            color.meta.blue = 0;
            color.meta.red = (0.681 + color.meta.red) / 2.0;
            color.meta.green = (0.681 + color.meta.green) / 2.0;
        }

        else if (rn0 < 2. / 3.)
        {
            c = jp[1];

            color.meta.green = 0;
            color.meta.red = (0.681 + color.meta.red) / 2.0;
            color.meta.blue = (0.681 + color.meta.blue) / 2.0;
        }

        else
        {
            c = jp[2];

            color.meta.red /= .5;
            color.meta.green = (0.681 + color.meta.green) / 2.0;
            color.meta.blue = (0.681 + color.meta.blue) / 2.0;
        }

        ct_complex d = z - c;
        ct_complex root = sqrt(d);


        if (i > 100)
        {
            ct_all_plotting_buddha(plot, z, z, 3);

            ct_plot_add(plot, z, &color);
        }

        // next iterate
        z = root;

        // Still, only two roots with 50/50
        if (rn1 > .5)
        {
            z = -root;

            color.meta.blue = 0;
            color.meta.red = (0.681 + color.meta.red) / 2.0;
        }

        else
        {
            color.meta.red = 0;
            color.meta.blue = (0.681 + color.meta.blue) / 2.0;
        }

        if (!(i % (n / 3)))
        {
            printf("rendering: %lu of %lu\r", i + 1, n);
        }
    }

    printf("rendering: %lu of %lu\n", n, n);
}





// slow, so what for now... ;^)
void ct_circle(
    struct ct_plot* const plot,
    ct_complex c,
    double radius,
    unsigned int n
) {
    double abase = 6.2831853071 / n;

    for (unsigned int i = 0; i < n; ++i)
    {
        double angle = abase * i;

        ct_complex z = {
            c.real() + cos(angle) * radius,
            c.imag() + sin(angle) * radius
        };

        struct ct_rgb rgb = { 255, 255, 255 };

        ct_plot_point(plot, z, &rgb);
    }
}


int main(void)
{
    struct ct_canvas canvas;

    if (ct_canvas_create(&canvas, CT_WIDTH, CT_HEIGHT))
    {
        ct_complex plane_origin = { 0, 0 };
        double plane_radius = 2.0;

        struct ct_axes axes = ct_axes_from_point(plane_origin, plane_radius);

        struct ct_plot plot;
        ct_plot_init(&plot, &axes, &canvas);


        ct_ifs(&plot, CT_N);

        //ct_para(&plot, plane_origin, 1.0);

        // Our unit circle
        ct_circle(&plot, { 0, 0 }, 1.0, 2048);
        ct_circle(&plot, { 2, 0 }, 2.0, 2048);
        ct_circle(&plot, { -2, 0 }, 2.0, 2048);
        ct_circle(&plot, { 0, 2 }, 2.0, 2048);
        ct_circle(&plot, { 0, -2 }, 2.0, 2048);

        ct_canvas_log_density_post_processing(&canvas);

        ct_canvas_save_ppm(&canvas, "ct_plane.ppm");

        ct_canvas_destroy(&canvas);

        {
            std::system("\"C:\\Program Files\\GIMP 2\\bin\\gimp-2.8.exe\" ct_plane.ppm");
        }

        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
