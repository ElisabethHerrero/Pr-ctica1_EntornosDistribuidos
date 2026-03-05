#include <iostream>
#include <map>
#include <future>
#include <vector>

using namespace std;

// Calcula el área multiplicando largo por ancho
int calcular_area(int largo, int ancho) {
    return largo * ancho;
}

int main() {

    // Definición de las zonas con sus dimensiones
    map<string, pair<int, int>> zonas = {
        {"Zona 1", {500, 150}},
        {"Zona 2", {480, 101}},
        {"Zona 3", {309, 480}},
        {"Zona 4", {90, 220}}
    };

    // Tasa de limpieza (1000 cm²/segundo)
    double tasa_limpieza = 1000.0;

    map<string, int> areas;

    // Vector para almacenar futures
    vector<pair<string, future<int>>> tareas;

    // Lanzar cálculos concurrentes
    for (auto& zona : zonas) {
        tareas.push_back({
            zona.first,
            async(launch::async, calcular_area, zona.second.first, zona.second.second)
            });
    }

    // Recoger resultados
    for (auto& tarea : tareas) {
        try {
            int area = tarea.second.get();
            areas[tarea.first] = area;
            cout << tarea.first << ": " << area << " cm²" << endl;
        }
        catch (exception& e) {
            cout << tarea.first << " generó una excepción: " << e.what() << endl;
        }
    }

    // Calcular superficie total
    int superficie_total = 0;
    for (auto& a : areas) {
        superficie_total += a.second;
    }

    // Tiempo de limpieza
    double tiempo_limpieza = superficie_total / tasa_limpieza;

    cout << "\nSuperficie total a limpiar: " << superficie_total << " cm²" << endl;
    cout << "Tiempo estimado de limpieza: " << tiempo_limpieza << " segundos" << endl;

    return 0;
}