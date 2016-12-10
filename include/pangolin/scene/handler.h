#pragma once

#include <pangolin/scene/object.h>
#include <pangolin/handler/handler.h>

namespace pangolin {

inline void gluPickMatrix(
    GLdouble x, GLdouble y,
    GLdouble width, GLdouble height,
    GLint viewport[4])
{
    GLfloat m[16];
    GLfloat sx, sy;
    GLfloat tx, ty;
    sx = viewport[2] / width;
    sy = viewport[3] / height;
    tx = (viewport[2] + 2.0 * (viewport[0] - x)) / width;
    ty = (viewport[3] + 2.0 * (viewport[1] - y)) / height;
#define M(row, col) m[col*4+row]
    M(0, 0) = sx;
    M(0, 1) = 0.0;
    M(0, 2) = 0.0;
    M(0, 3) = tx;
    M(1, 0) = 0.0;
    M(1, 1) = sy;
    M(1, 2) = 0.0;
    M(1, 3) = ty;
    M(2, 0) = 0.0;
    M(2, 1) = 0.0;
    M(2, 2) = 1.0;
    M(2, 3) = 0.0;
    M(3, 0) = 0.0;
    M(3, 1) = 0.0;
    M(3, 2) = 0.0;
    M(3, 3) = 1.0;
#undef M
    glMultMatrixf(m);
}



struct SceneHandler : public Handler3D
{
    SceneHandler(
        Renderable& scene,
        OpenGlRenderState& cam_state
    ) : Handler3D(cam_state), scene(scene)
    {

    }

    void ProcessHitBuffer(GLint hits, GLuint* buf, std::map<GLuint, Interactive*>& hit_map )
    {
        GLuint* closestNames = 0;
        GLuint closestNumNames = 0;
        GLuint closestZ = std::numeric_limits<GLuint>::max();
        for (int i = 0; i < hits; i++) {
            if (buf[1] < closestZ) {
                closestNames = buf + 3;
                closestNumNames = buf[0];
                closestZ = buf[1];
            }
            buf += buf[0] + 3;
        }
        for (unsigned int i = 0; i < closestNumNames; i++) {
            const int pickId = closestNames[i];
            hit_map[pickId] = InteractiveIndex::I().Find(pickId);
        }
    }

    void ComputeHits(pangolin::View& view,
                     const pangolin::OpenGlRenderState& cam_state,
                     int x, int y, int grab_width,
                     std::map<GLuint, Interactive*>& hit_objects )
    {
        // Get views viewport / modelview /projection
        GLint viewport[4] = {view.v.l, view.v.b, view.v.w, view.v.h};
        pangolin::OpenGlMatrix mv = cam_state.GetModelViewMatrix();
        pangolin::OpenGlMatrix proj = cam_state.GetProjectionMatrix();

        // Prepare hit buffer object
        const unsigned int MAX_SEL_SIZE = 64;
        GLuint vSelectBuf[MAX_SEL_SIZE];
        glSelectBuffer(MAX_SEL_SIZE, vSelectBuf);

        // Load and adjust modelview projection matrices
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPickMatrix(x, y, grab_width, grab_width, viewport);
        proj.Multiply();
        glMatrixMode(GL_MODELVIEW);
        mv.Load();

        // Render scenegraph in 'select' mode
        glRenderMode(GL_SELECT);
        glInitNames();
        RenderParams select;
        select.render_mode = GL_SELECT;
        scene.Render(select);
        glFlush();

        GLint nHits = glRenderMode(GL_RENDER);
        //    std::cout << " -- Number of Hits are: " << nHits << std::endl;
        //    std::cout << " -- size of hitobjects: " << hit_objects.size() << std::endl;
        if (nHits > 0) {
            ProcessHitBuffer(nHits, vSelectBuf, hit_objects);
        }
    }

    void Mouse(pangolin::View& view, pangolin::MouseButton button,
               int x, int y, bool pressed, int button_state)
    {
        GetPosNormal(view, x, y, p, Pw, Pc, n);
        bool handled = false;

        if (pressed) {
            m_selected_objects.clear();
            ComputeHits(view, *cam_state, x, y, 2*hwin+1, m_selected_objects);
        }

        for (auto kv : m_selected_objects)
        {
            Interactive* ir = dynamic_cast<Interactive*>(kv.second);
            handled |= ir && ir->Mouse(
                        button,
                        Eigen::Map<Eigen::Matrix<GLdouble, 3, 1> >(p).cast<double>(),
                        Eigen::Map<Eigen::Matrix<GLdouble, 3, 1> >(Pw).cast<double>(),
                        Eigen::Map<Eigen::Matrix<GLdouble, 3, 1> >(n).cast<double>(),
                        pressed, button_state, kv.first);
        }

        if (!handled) {
            Handler3D::Mouse(view, button, x, y, pressed, button_state);
        }
    }

    void MouseMotion(pangolin::View& view, int x, int y, int button_state)
    {
        GetPosNormal(view, x, y, p, Pw, Pc, n);
        bool handled = false;
        for (auto kv : m_selected_objects)
        {
            Interactive* ir = dynamic_cast<Interactive*>(kv.second);

            handled |= ir && ir->MouseMotion(
                        Eigen::Map<Eigen::Matrix<GLdouble, 3, 1> >(p).cast<double>(),
                        Eigen::Map<Eigen::Matrix<GLdouble, 3, 1> >(Pw).cast<double>(),
                        Eigen::Map<Eigen::Matrix<GLdouble, 3, 1> >(n).cast<double>(),
                        button_state, kv.first);
        }
        if (!handled) {
            pangolin::Handler3D::MouseMotion(view, x, y, button_state);
        }
    }

    void Special(pangolin::View& view, pangolin::InputSpecial inType,
                 float x, float y, float p1, float p2, float p3, float p4,
                 int button_state)
    {
        GetPosNormal(view, x, y, p, Pw, Pc, n);

        bool handled = false;

        if (inType == pangolin::InputSpecialScroll)
        {
            m_selected_objects.clear();
            ComputeHits(view, *cam_state, x, y, 2*hwin+1, m_selected_objects);

            const MouseButton button = p2 > 0 ? MouseWheelUp : MouseWheelDown;

            for (auto kv : m_selected_objects)
            {
                Interactive* ir = dynamic_cast<Interactive*>(kv.second);
                handled |= ir && ir->Mouse(
                            button,
                            Eigen::Map<Eigen::Matrix<GLdouble, 3, 1>>(p).cast<double>(),
                            Eigen::Map<Eigen::Matrix<GLdouble, 3, 1>>(Pw).cast<double>(),
                            Eigen::Map<Eigen::Matrix<GLdouble, 3, 1>>(n).cast<double>(),
                            true, button_state, kv.first);
            }
        }

        if (!handled) {
            pangolin::Handler3D::Special(view, inType, x, y,
                                         p1, p2, p3, p4, button_state);
        }
    }

    std::map<GLuint, Interactive*> m_selected_objects;
    Renderable& scene;
    unsigned int grab_width;
};

}
