#include <Bus.hpp>
#include <getopt.h>
#include <unistd.h>
#include <EE.hpp>
#include <gs/gsrenderer.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <gs/gs.hpp>

void error_callback( int error, const char *msg ) {
    std::string s;
    s = " [" + std::to_string(error) + "] " + msg + '\n';
    std::cerr << s << std::endl;
}

int main(int argc, char** argv)
{
    if(!glfwInit())
    {
        printf("[Main]: Error initing GLFW\n");
        return -1;
    }
    glfwSetErrorCallback(error_callback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "PS2 Emulator", NULL, NULL);
    if (window == NULL)
    {
        printf("[MAIN] Error creating main window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);
    });

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return -1;
    if (argc < 2)
    {
        printf("Usage: %s [BIOS] {ELF/CDROM}\n", argv[0]);
        return 1;
    }

    gs::GraphicsSynthesizer GS;
    Bus* bus = new Bus(argv[1], &GS);
    EmotionEngine* cpu = new EmotionEngine(bus);
    INTC* intc = cpu->getIntc();
    Timers* timers = cpu->getTimers();
    bus->attachIntc(intc);
    bus->attachTimers(timers);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClearDepth(0.0);
    glClear(GL_DEPTH_BUFFER_BIT);

    glfwSwapBuffers(window);
    glfwPollEvents();

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClearDepth(0.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        uint32_t total_cycles = 0;
        bool vblank_started = false;
        while (total_cycles < 4919808)
        {
            uint32_t cycles = 32;
            cpu->Clock(cycles);
            timers->tick(cycles / 2);

            if (intc->int_pending())
            {
                printf("[EE]: Interrupt");
                cpu->exception(EmotionEngine::Exception::Interrupt, true);
            }

            total_cycles += 32;

            if (!vblank_started && total_cycles >= 4498432)
            {
                vblank_started = true;
                GS.priv_regs.csr.vsint = true;

                printf("[MAIN]: Rendering frame\n");

                GS.renderer.render();

                GS.priv_regs.csr.field = !GS.priv_regs.csr.field;

                if (!(GS.priv_regs.imr & 0x800))
                    intc->trigger(Interrupt::INT_GS);
                
                intc->trigger(Interrupt::INT_VB_ON);
            }
        }

	printf("[MAIN]: VBLANK OFF\n");

        intc->trigger(Interrupt::INT_VB_OFF);
        vblank_started = false;
        GS.priv_regs.csr.vsint = false;

	printf("[MAIN]: Swapping Buffers\n");

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    return 0;
}
