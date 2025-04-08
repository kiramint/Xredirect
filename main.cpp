#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <opencv2/opencv.hpp>
#include <utility>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

class X11RedirectException : std::exception {
public:
    explicit X11RedirectException(std::string message): message(std::move(message)) {
    }

    [[nodiscard]] const char *what() const noexcept override {
        return message.c_str();
    }

private:
    std::string message;
};

class X11Redirect {
public:
    X11Redirect(std::string DISPLAY, int captureIntervalMs): DISPLAY(std::move(DISPLAY)),
                                                             captureIntervalMs(captureIntervalMs) {
        display = XOpenDisplay(DISPLAY.c_str());
        if (display == nullptr) {
            std::cerr << "Cannot connect to X server: " << DISPLAY << std::endl;
            throw X11RedirectException("Cannot connect to X server: " + DISPLAY);
        }
        screen = DefaultScreenOfDisplay(display);
        _screenWidth = DisplayWidth(display, DefaultScreen(display));
        _screenHeight = DisplayHeight(display, DefaultScreen(display));
    }

    ~X11Redirect() {
        if (display != nullptr) {
            XCloseDisplay(display);
        }
    }

    void xClick(int x, int y) {
        if (display != nullptr) {
            XTestFakeMotionEvent(display, -1, x, y, CurrentTime);
            XTestFakeButtonEvent(display, 1, True, CurrentTime);
            XTestFakeButtonEvent(display, 1, False, CurrentTime);
            XFlush(display);
        } else {
            std::cerr << "Cannot connect to X server: " << DISPLAY << std::endl;
            throw X11RedirectException("Cannot connect to X server: " + DISPLAY);
        }
    }

    cv::Mat xCapture() {
        image = XGetImage(display, RootWindow(display, DefaultScreen(display)),
                          0, 0, _screenWidth, _screenHeight, AllPlanes, ZPixmap);
        if (image == nullptr) {
            std::cerr << "Failed to capture screen image" << std::endl;
            throw X11RedirectException("Failed to capture screen image");
        }
        cv::Mat frame = XImageToMat(image);
        return frame.clone();
    }

private:
    int _screenWidth{};
    int _screenHeight{};
    int captureIntervalMs = 1000;
    std::string DISPLAY;

    Display *display = nullptr;
    Screen *screen = nullptr;
    XImage *image = nullptr;

    static cv::Mat XImageToMat(XImage *image) {
        int width = image->width;
        int height = image->height;
        int bpp = image->bits_per_pixel;

        if (bpp != 32) {
            std::cerr << "Unsupported bits_per_pixel: " << bpp << std::endl;
            throw X11RedirectException("Unsupported bits_per_pixel: " + bpp);
        }
        cv::Mat mat(height, width, CV_8UC4);
        for (int y = 0; y < height; ++y) {
            memcpy(mat.ptr(y), image->data + y * image->bytes_per_line, width * 4);
        }
        cv::Mat matBGR;
        cv::cvtColor(mat, matBGR, cv::COLOR_BGRA2BGR);
        return matBGR;
    }
};

int main() {
    std::cout << "Hello, World!" << std::endl;

    X11Redirect xrd(":0", 33);
    auto frame = xrd.xCapture();
    cv::imwrite("Debug.jpg", frame);
    xrd.xClick(1839, 60);

    return 0;
}
