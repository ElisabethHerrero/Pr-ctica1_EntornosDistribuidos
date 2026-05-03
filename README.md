# Práctica1_EntornosDistribuidos

## Integrantes del grupo
- María Rasero  
- Lara Durán  
- Elisabeth Herrero  

---

## 1. Enfoque distribuido utilizado

El proyecto implementa una **arquitectura por capas en C++** que simula un entorno distribuido para la gestión de varias Roombas trabajando sobre distintas zonas de una habitación.

### Capa de presentación
La clase `ScreenManager` actúa como vista y controlador visual. Se encarga de:
- Pantalla de inicio  
- Configuración de Roombas  
- Animación de limpieza  
- Pantalla final de resultados  

Además, representa:
- Zonas y obstáculos  
- Rastro de limpieza  
- Estado de cada robot en tiempo real  

Esta capa permite visualizar la concurrencia y comprobar la correcta coordinación entre robots.

---

### Capa de servicios
La clase `CleaningService` centraliza la lógica concurrente:
- Usa un hilo principal de trabajo  
- Asigna zonas pendientes  
- Controla movimiento  
- Recalcula rutas  
- Libera robots al terminar una zona  

Cada `Froomba`:
- Funciona de forma independiente  
- Mantiene su propio estado (velocidad, radio, trayectoria, etc.)  

La clase `EventService` implementa un patrón **publicación-suscripción**, permitiendo:
- Notificar inicio/fin de limpieza  
- Informar de zonas completadas  
- Reducir acoplamiento entre módulos  

---

### Capa de datos
- `Zone`: modela habitaciones, celdas, obstáculos y progreso  
- `Database`: persistencia ligera mediante fichero de log  

Se mantiene separación entre modelo y almacenamiento, permitiendo futuras mejoras como:
- Uso de SQLite  
- Integración con servidor remoto  

---

## 2. Elección de herramientas y concurrencia

Se utilizan:
- `std::thread`
- `std::atomic`
- `std::mutex`

Esto permite:
- Ejecutar lógica en paralelo con la interfaz  
- Compartir estado de forma segura  
- Evitar condiciones de carrera  

### Detalles clave
- `Froomba`: usa variables atómicas para estado interno  
- `Zone`: protegida con mutex para acceso concurrente  
- Sincronización correcta entre lógica y visualización  

### Conceptos aplicados
- Reparto de carga  
- Sincronización  
- Comunicación por eventos  
- Tolerancia básica a fallos (recalcular rutas, recolocación)  

---

## 3. Posibles mejoras y extensiones futuras

### Persistencia avanzada
- Sustituir logs por base de datos real  
- Registrar sesiones completas:
  - Hora de inicio/fin  
  - Número de robots  
  - Métricas por zona  

---

### Sistema distribuido real
- Arquitectura cliente-servidor  
- Comunicación **peer-to-peer** entre robots  

Ejemplos:
- Servidor que reparte zonas  
- Robots que comparten información de rutas  

---

### Mejoras técnicas
- Planificación avanzada  
- Detección de colisiones  
- Carga dinámica de mapas  
- Mejor gestión de errores  
- Cierre seguro de hilos  

---

## Conclusión

El sistema actual proporciona una base sólida:
- Arquitectura por capas  
- Coordinación de agentes concurrentes  
- Visualización en tiempo real  
- Preparado para futuras ampliaciones  
