// pacman_roomba_level.cpp

#include <iostream>
#include <map>
#include <future>
#include <vector>
#include <cmath>

using namespace std;

class Vector {
public:
    double x;
    double y;
    double thresh;

    Vector(double x = 0, double y = 0) : x(x), y(y), thresh(0.000001) {}

    Vector operator+(const Vector& other) const {
        return Vector(x + other.x, y + other.y);
    }

    Vector operator-(const Vector& other) const {
        return Vector(x - other.x, y - other.y);
    }

    Vector operator*(double scalar) const {
        return Vector(x * scalar, y * scalar);
    }

    double magnitude() const {
        return sqrt(x * x + y * y);
    }

    double magnitudeSquared() const {
        return x * x + y * y;
    }

    int area() const {
        return x * y;
    }

    bool equals(const Vector& other) const {
        return abs(x - other.x) < thresh && abs(y - other.y) < thresh;
    }

    friend ostream& operator<<(ostream& os, const Vector& v) {
        os << "<" << v.x << ", " << v.y << ">";
        return os;
    }
};

int calcular_area(Vector v) {
    return v.area();
}

int main() {

    // Zonas representadas como vectores (largo, ancho)
    map<string, Vector> zonas = {
        {"Zona 1", Vector(500,150)},
        {"Zona 2", Vector(480,101)},
        {"Zona 3", Vector(309,480)},
        {"Zona 4", Vector(90,220)}
    };

    double tasa_limpieza = 1000.0;

    map<string, int> areas;
    vector<pair<string, future<int>>> tareas;

    // Cálculo concurrente de áreas (estilo Roomba)
    for (auto& zona : zonas) {
        tareas.push_back({
            zona.first,
            async(launch::async, calcular_area, zona.second)
            });
    }

    // Recoger resultados
    for (auto& tarea : tareas) {
        try {
            int area = tarea.second.get();
            areas[tarea.first] = area;

            cout << tarea.first
                << " dimensiones " << zonas[tarea.first]
                << " -> area: " << area << " cm²" << endl;
        }
        catch (exception& e) {
            cout << "Error en " << tarea.first << ": " << e.what() << endl;
        }
    }

    int superficie_total = 0;

    for (auto& a : areas) {
        superficie_total += a.second;
    }

    double tiempo_limpieza = superficie_total / tasa_limpieza;

    cout << "\nSuperficie total: " << superficie_total << " cm²" << endl;
    cout << "Tiempo limpieza: " << tiempo_limpieza << " segundos" << endl;
    cout << "Tiempo limpieza: " << tiempo_limpieza / 60 << " minutos" << endl;

    return 0;
}