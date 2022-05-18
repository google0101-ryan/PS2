#include <Bus.hpp>
#include <getopt.h>
#include <unistd.h>
#include <EE.hpp>
#include <gs/gsrenderer.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <gs/gs.hpp>
#include <dmac.hpp>
#include <iop/iop.hpp>
#include <iop/iop_intc.hpp>

void error_callback( int error, const char *msg ) {
    std::string s;
    s = " [" + std::to_string(error) + "] " + msg + '\n';
    std::cerr << s << std::endl;
}

IOP* iop;

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
    GIF* gif = new GIF(&GS);
    DMAController* dmac = new DMAController(bus, cpu);
    bus->vu[0] = new VectorUnit(cpu);
    bus->vu[1] = new VectorUnit(cpu);
    INTC* intc = cpu->getIntc();
    Timers* timers = cpu->getTimers();
    SIO2* sio2 = new SIO2(bus);
    iop = new IOP(bus);
    IOP_INTC* iop_intc = new IOP_INTC(iop);
    bus->vif[0] = new VIF(bus, 0);
    bus->vif[1] = new VIF(bus, 1);
    bus->attachIntc(intc);
    bus->attachTimers(timers);
    bus->attachGIF(gif);
    bus->dmac = dmac;
    bus->sio2 = sio2;

    iop->reset();
    iop->set_disassembly(true);
    iop_intc->reset();

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

            cycles /= 2;
            dmac->tick(cycles);
            bus->vif[0]->tick(cycles);
            bus->vif[1]->tick(cycles);
            gif->tick(cycles);

            cycles /= 4;
            iop->run(cycles);

            total_cycles += 32;

            if (!vblank_started && total_cycles >= 4498432)
            {
                vblank_started = true;
                GS.priv_regs.csr.vsint = true;

                GS.renderer.render();

                GS.priv_regs.csr.field = !GS.priv_regs.csr.field;

                if (!(GS.priv_regs.imr & 0x800))
                    intc->trigger(Interrupt::INT_GS);
                
                intc->trigger(Interrupt::INT_VB_ON);
                iop_intc->assert_irq(0);
            }
        }

	    intc->trigger(Interrupt::INT_VB_OFF);
        iop_intc->assert_irq(11);
        vblank_started = false;
        GS.priv_regs.csr.vsint = false;

	    glfwSwapBuffers(window);
        glfwPollEvents();
    }
    return 0;
}
