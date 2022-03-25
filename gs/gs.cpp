#include <gs/gs.hpp>
#include <gs/gsrenderer.hpp>
#include <cassert>
#include <unordered_map>
#include <glad/glad.h>
#include <climits>

using namespace gs;

static const char* PRIV_REGS[] =
{
	"PMODE", "SMODE1", "SMODE2",
	"SRFSH", "SYNCH1", "SYNCH2",
	"SYNCV", "DISPFB1", "DISPLAY1",
	"DISPFB2", "DISPLAY2", "EXTBUF",
	"EXTDATA", "EXTWRITE", "BGCOLOR",
	"GS_CSR", "GS_IMR", "BUSDIR",
	"SIGLBLID"
};

std::unordered_map<int, const char*> REGS =
{
	{ 0x00, "PRIM" },
	{ 0x01, "RGBAQ" },
	{ 0x02, "ST" },
	{ 0x03, "UV" },
	{ 0x04, "XYZF2" },
	{ 0x05, "XYZ2" },
	{ 0x06, "TEX0_1" }, { 0x07, "TEX0_2" },
	{ 0x08, "CLAMP_1" }, { 0x9, "CLAMP_2" },
	{ 0x0a, "FOG" },
	{ 0x0c, "XYZF3" },
	{ 0x0d, "XYZ3" },
	{ 0x14, "TEX1_1" }, { 0x15, "TEX_2" },
	{ 0x16, "TEX2_1" }, { 0x17, "TEX2_2"},
	{ 0x18, "XYOFFSET_1" }, { 0x19, "XYOFFSET_2"},
	{ 0x1A, "PRMODECONT" },
	{ 0x1B, "PRMODE" },
	{ 0x1C, "TEXCLUT" },
	{ 0x22, "SCANMSK" },
	{ 0x34, "MIPTBP1_1" }, { 0x35, "MIPTBP1_2" },
	{ 0x36, "MIPTBP2_1" }, { 0x37, "MIPTBP2_2" },
	{ 0x3B, "TEXA" },
	{ 0x3D, "FOGCOL" },
	{ 0x3F, "TEXFLUSH" },
	{ 0x40, "SCISSOR_1" }, { 0x41, "SCISSOR_2" },
	{ 0x42, "ALPHA_1"}, { 0x43, "ALPHA_2" },
	{ 0x44, "DIMX" },
	{ 0x45, "DTHE" },
	{ 0x46, "COLCLAMP" },
	{ 0x47, "TEST_1" }, { 0x48, "TEST_2" },
	{ 0x49, "PABE" },
	{ 0x4A, "FBA_1" }, { 0x4b, "FBA_2" },
	{ 0x4C, "FRAME_1"}, { 0x4d, "FRAME_2" },
	{ 0x4E, "ZBUF_1" }, { 0x4f, "ZBUF_2" },
	{ 0x50, "BITBLTBUF" },
	{ 0x51, "TRXPOS" },
	{ 0x52, "TRXREG" },
	{ 0x53, "TRXDIR" },
	{ 0x54, "HWREG" },
	{ 0x60, "SIGNAL" },
	{ 0x61, "FINISH" },
	{ 0x62, "LABEL" }
};

namespace gs
{
	GraphicsSynthesizer::GraphicsSynthesizer()
	{
		/* Allocate VRAM */
		vram = new Page[512];
	}

	GraphicsSynthesizer::~GraphicsSynthesizer()
	{
		delete[] vram;
	}

	uint64_t GraphicsSynthesizer::read_priv(uint32_t addr)
	{
		bool group = addr & 0xf000;
		uint32_t offset = ((addr >> 4) & 0xf) + 15 * group;
		auto ptr = (uint64_t*)&priv_regs + offset;

		//fmt::print("[GS] Reading {:#x} from {}\n", *ptr, PRIV_REGS[offset]);
		/* Only CSR and SIGLBLID are readable! */
		assert(offset == 15 || offset == 18);

		return *ptr;
	}

	void GraphicsSynthesizer::write_priv(uint32_t addr, uint64_t data)
	{
		bool group = addr & 0xf000;
		uint32_t offset = ((addr >> 4) & 0xf) + 15 * group;
		auto ptr = (uint64_t*)&priv_regs + offset;

		*ptr = data;

		switch (offset)
		{
		case 15:
		{
			if (data & 0x8)
				priv_regs.csr.vsint = false;
			break;
		}
		}
	}

	void GraphicsSynthesizer::write(uint16_t addr, uint64_t data)
	{
		auto context = addr & 1;
		switch (addr)
		{
		case 0x0:
			prim = data;
			break;
		case 0x1:
			rgbaq.value = data;
			break;
		case 0x2:
			st = data;
			break;
		case 0x3:
			uv = data;
			break;
		case 0x4:
			xyzf2.value = data;
            submit_vertex_fog(xyzf2, true);
			break;
		case 0x5:
			xyz2.value = data;
            submit_vertex(xyz2, true);
			break;
		case 0x6:
		case 0x7:
			tex0[context] = data;
			break;
		case 0x8:
		case 0x9:
			clamp[context] = data;
			break;
		case 0xa:
			fog = data;
			break;
        case 0xd:
            xyz3.value = data;
            submit_vertex(xyz3, false);
            break;
		case 0x14:
		case 0x15:
			tex1[context] = data;
			break;
		case 0x16:
		case 0x17:
			tex2[context] = data;
			break;
		case 0x18:
		case 0x19:
			xyoffset[context].value = data;
			break;
		case 0x1a:
			prmodecont = data;
			break;
		case 0x1b:
			prmode = data;
			break;
		case 0x1c:
			texclut = data;
			break;
		case 0x22:
			scanmsk = data;
			break;
		case 0x34:
		case 0x35:
			miptbp1[context] = data;
			break;
		case 0x36:
		case 0x37:
			miptbp2[context] = data;
			break;
		case 0x3b:
			texa = data;
			break;
		case 0x3d:
			fogcol = data;
			break;
		case 0x3f:
			texflush = data;
			break;
		case 0x40:
		case 0x41:
			scissor[context] = data;
			break;
		case 0x42:
		case 0x43:
			alpha[context] = data;
			break;
		case 0x44:
			dimx = data;
			break;
		case 0x45:
			dthe = data;
			break;
		case 0x46:
			colclamp = data;
			break;
		case 0x47:
		case 0x48:
			test[context] = data;
			
			/* Flush renderer */
			renderer.render();
			switch ((test[context] >> 17) & 0x3)
			{
			case 0:
				glDepthFunc(GL_NEVER);
				break;
			case 1:
				glDepthFunc(GL_GEQUAL);
				break;
			case 3:
				glDepthFunc(GL_GREATER);
				break;
			default:
                printf("[GS] Unknown depth function selected!\n");
                exit(1);
			}

			break;
		case 0x49:
			pabe = data;
			break;
		case 0x4a:
		case 0x4b:
			fba[context] = data;
			break;
		case 0x4c:
		case 0x4d:
			frame[context] = data;
			break;
		case 0x4e:
		case 0x4f:
			zbuf[context] = data;
			break;
		case 0x50:
			bitbltbuf.value = data;
			break;
		case 0x51:
			trxpos.value = data;
			break;
		case 0x52:
			trxreg.value = data;
			break;
		case 0x53:
			trxdir = data;
			data_written = 0;
			break;
		default:
            printf("[GS] Writting 0x%lX to unknown address 0x%X\n", data, addr);
            exit(1);
		}
    }

	void GraphicsSynthesizer::write_hwreg(uint64_t data)
	{
		/* HWREG is only used for GIF -> VRAM transfers */
		if (trxdir != TRXDir::HostLocal)
		{
            printf("[GS] Write to HWREG with invalid transfer dir!\n");
            exit(1);
			return;
		}
		
		/* Useful for many things */
		uint32_t width_in_pages = bitbltbuf.dest_width;
		uint32_t width_in_pixels = trxreg.width;

		uint16_t format = bitbltbuf.dest_pixel_format;
		switch (format)
		{
		case PixelFormat::PSMCT32:
		{
			/* HWREG values are 64bit so they pack 2 pixels together */
			uint32_t* pixels = (uint32_t*)&data;
			for (int i = 0; i < 2; i++)
			{
				uint16_t x = data_written % width_in_pixels;
				uint16_t y = data_written / width_in_pixels;

				x += trxpos.dest_top_left_x;
				y += trxpos.dest_top_left_y;

				/* Calculate which page we are refering to */
				/* NOTE: x, y can refer to outside of the selected page (if their values are bigger than the page dimentions) */
				/* Update the page accordingly. Loop back if the page is over the page width */
				uint32_t page = bitbltbuf.dest_base / BLOCKS_PER_PAGE;
				page += (x / Page::PIXEL_WIDTH) % width_in_pages + (y / Page::PIXEL_HEIGHT) * width_in_pages;
				
				vram[page].write_psmct32(x, y, pixels[i]);
				data_written++;
			}
			
			break;
		}
		case PixelFormat::PSMCT16:
		{
			uint16_t* pixels = (uint16_t*)&data;
			for (int i = 0; i < 4; i++)
			{
				uint16_t x = data_written % width_in_pixels;
				uint16_t y = data_written / width_in_pixels;

				x += trxpos.dest_top_left_x;
				y += trxpos.dest_top_left_y;

				uint32_t page = bitbltbuf.dest_base / BLOCKS_PER_PAGE;
				page += (x / 64) % width_in_pages + (y / 64) * width_in_pages;

				vram[page].write_psmct16(x, y, pixels[i]);
				data_written++;
			}
			break;
		}
		default:
            printf("[GS] Unknown texture format 0x%X\n", format);
            exit(1);
		}

		/* Check if transfer has completed */
		if (data_written >= trxreg.width * trxreg.height)
		{
			data_written = 0;

			/* Deactivate TRXDIR */
			trxdir = TRXDir::None;
		}
	}

    void GraphicsSynthesizer::submit_vertex_fog(XYZF xyzf, bool draw_kick)
    {
        GSVertex vertex;
        vertex.x = xyzf.x;
        vertex.y = xyzf.y;
        vertex.z = xyzf.z;
        process_vertex(vertex, draw_kick);
    }

    void GraphicsSynthesizer::submit_vertex(XYZ xyz, bool draw_kick)
    {
        GSVertex vertex;
        vertex.x = xyz.x;
        vertex.y = xyz.y;
        vertex.z = xyz.z;
        process_vertex(vertex, draw_kick);
    }

    void GraphicsSynthesizer::process_vertex(GSVertex vertex, bool draw_kick)
	{
		/* Convert the primitive coords to window coords */
		vertex.x = (vertex.x - xyoffset[0].x_offset) / 16.0f;
		vertex.y = (vertex.y - xyoffset[0].y_offset) / 16.0f;

		/* Convert to OpenGL coords */
		vertex.x = (vertex.x / 320.0f) - 1.0f;
		vertex.y = 1.0f - (vertex.y / 112.0f);
        vertex.z = vertex.z / static_cast<float>(INT_MAX);
		
		/* Set color information */
		vertex.r = rgbaq.r / 255.0f;
		vertex.g = rgbaq.g / 255.0f;
		vertex.b = rgbaq.b / 255.0f;

		if (!vqueue.push(vertex))
		{
            /* Happens when we write to XYZ3 */
            vqueue.pop<GSVertex>();
            vqueue.push(vertex);
		}

        if (draw_kick)
        {
            switch(vqueue.size())
            {
            case 2:
                switch (prim & 0x7)
                {
                case Primitive::Sprite:
                    GSVertex v1, v2;
                    vqueue.read(&v1); vqueue.pop();
                    vqueue.read(&v2); vqueue.pop();
                    renderer.submit_sprite(v1, v2);
                    break;
                }
                break;
            case 3:
                switch (prim & 0x7)
                {
                case Primitive::Triangle:
                    GSVertex v;
                    for (int i = 0; i < 3; i++)
                    {
                        vqueue.read(&v);
                        bool f = vqueue.pop();

                        assert(f);

                        renderer.submit_vertex(v);
                    }
                    break;
                }
                break;
            }
        }
	}
}