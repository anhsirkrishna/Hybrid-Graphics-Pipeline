#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

class Window;

class Camera
{
public:
    Camera();

    glm::mat4 perspective(const float aspect);
    glm::mat4 view();

    void Update(float dt);

    void SetControlWindow(Window* _p_window);

    void DrawGUI();

    void SetEyePos(glm::vec3 _eye_pos);
    void SetSpin(float _spin);
    void SetTilt(float _tilt);
    bool WasUpdated() const;
private:
    float ry;
    float front;
    float back;

    float spin;
    float tilt;

    float oldSpin;

    glm::vec3 eye;

    bool updated;
    bool update_mouse;

    float posx = 0.0;
    float posy = 0.0;

    Window* p_control_window;

    void HandleKeyInputs(float dt);
    void HandleMouseInputs(float dt);

    void mouseMove(const float x, const float y);
    void setMousePosition(const float x, const float y);
    void wheel(const int dir);
};
