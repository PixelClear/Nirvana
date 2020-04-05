#include <stdio.h>
#include <assert.h>
#include <GLFW/glfw3.h>

int main()
{
    int rc = glfwInit();
    assert(rc);

    GLFWwindow* window = glfwCreateWindow(1024, 786, "Nirvana", 0, 0);
    assert(window);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
}