Entrega 1: Diseño de dominio y arquitectura distribuida
1. Modelo de dominio
La entidad principal del sistema es Partida, definida dentro de DomainModels, que representa el estado global del juego. En esta clase se encuentra toda la información relevante de una sesión, como el identificador único, el alias del jugador, el escenario seleccionado, los fondos disponibles, la fecha de creación, la ronda actual y el estado de la partida.

A partir de dicha entidad se estructura el resto del modelo. Una Partida contiene una lista de enclaves (Enclave) y una lista de registros de evento (RegistroEvento), también definidos en DomainModels. Los enclaves representan las zonas gestionadas por el jugador y almacenan información como su nombre, tipo, hectáreas disponibles, suministros o visitantes. 
Cada enclave incluye a su vez un conjunto de instalaciones (Instalacion), que se tratan de los recintos construidos dentro del mismo. Las instalaciones recogen información como su código, nombre, tipo, coste de construcción, capacidad o características del entorno en el que operan. Además, cada instalación contiene una lista de criaturas (Criatura), estableciendo así la relación directa con los elementos vivos del sistema.

Las criaturas, también dentro de DomainModels, representan los seres gestionados durante la partida. Cada una dispone de atributos que establecen su comportamiento y características, como su edad, salud, tipo de alimentación o estado actual.

Por último, los registros de evento (RegistroEvento) permiten almacenar un historial de acciones y sucesos relevantes, como la creación de partidas, construcción de instalaciones, alimentación, reproducción, traslados o errores de comunicación, lo cual facilita saber qué ha ocurrido en el sistema y en qué orden, proporcionando trazabilidad.
2. Enumerados definidos
Dentro de Enums, en DomainModels, se definen distintos enumerados, que permiten limitar los valores posibles del sistema a opciones predefinidas y evitar el uso de cadenas de texto. 

Estos enumerados se utilizan directamente en las entidades del modelo, lo que permite controlar estados y tipos de forma segura. Por ejemplo, EscenarioJuego define los escenarios disponibles, EstadoPartida y EstadoCriatura controlan los estados posibles, y TipoEnclave o TipoInstalacion limitan las configuraciones válidas.

Además, se incluyen otros enums como Medio, TipoAlimentacion o NivelAdquisitivo, que definen características específicas del sistema, y otros como TipoEvento o SeveridadEvento, que permiten clasificar los eventos registrados.

El uso de estos enumerados mejora la claridad del código y facilita su mantenimiento, ya que todos los valores posibles se definen en un único lugar del código.
3. DTOs definidos
Para separar el modelo de dominio de la comunicación externa, se definen distintos DTOs dentro de DTOs:

En Common se encuentran los DTOs que representan estructuras básicas del sistema, como criaturas (CriaturaDTO), enclaves (EnclaveDTO), instalaciones (InstalacionDTO) y eventos (EventoDTO). Estos objetos permiten enviar información al cliente sin exponer directamente las entidades del dominio.

En Requests se definen los objetos que representan las peticiones que Unity envía al servidor, como CrearPartida, EjecutarRonda y GuardarPartida, encapsulando los datos necesarios para ejecutar acciones.

En Responses se definen los objetos que el servidor devuelve al cliente, como OperacionResultado, PartidaDetalle y PartidaResumen, permitiendo estructurar las respuestas de forma clara.

Esta separación entre dominio y DTOs reduce el acoplamiento entre componentes y facilita la evolución del sistema.
4. Proyectos y estructura inicial de la solución
La solución se organiza en dos proyectos principales que separan claramente la lógica del sistema de su interfaz.

El proyecto Servidor_dune contiene el API desarrollado en ASP.NET Core y actúa como servidor. En él se ubica la lógica del sistema, organizando los distintos elementos en Controllers, DomainModels, DTOs y Logging.

Los Controllers actúan como punto de entrada de las peticiones externas, conectando el cliente con el sistema. En DomainModels se definen las entidades, como partidas (Partida), enclaves (Enclave), instalaciones (Instalacion), criaturas (Criatura) y registros de evento (RegistroEvento), así como los enumerados. Por otro lado, DTOs recoge los objetos empleados en la comunicación. El sistema de Logging permite registrar eventos y verificar el funcionamiento del sistema.

El proyecto Unity_Dune actúa como cliente y proporciona la interfaz visual. Desde él se consulta el estado del sistema y se envían peticiones al servidor para ejecutar acciones. Además, la solución incluye elementos propios de una aplicación .NET, como archivos de configuración (appsettings.json), el archivo Program.cs o herramientas como Servidor_dune.http para probar el API.
5. Arquitectura distribuida propuesta
La arquitectura sigue un modelo cliente-servidor, en el que Unity_Dune actúa como cliente y Servidor_dune como servidor.

Esta estructura separa la interfaz de usuario de la lógica de negocio. Unity no accede directamente a las entidades definidas en DomainModels, como Partida, Enclave o Criatura, ni a la persistencia, sino que realiza peticiones a los Controllers del servidor.

El servidor procesa estas peticiones, aplica la lógica correspondiente sobre el modelo de dominio y devuelve respuestas estructuradas mediante los DTOs. El sistema guarda y carga los datos usando ficheros JSON, lo que permite mantener el estado de la partida sin necesidad de una base de datos externa. 
6. Componentes y responsabilidades
El sistema se divide en varios componentes con responsabilidades claramente definidas. El cliente Unity_Dune gestiona la interacción con el usuario, actuando como centro de mando. Desde él se envían peticiones al servidor y se muestran los resultados.

El servidor Servidor_dune, se divide en varias capas cada una con su función. La capa de Presentación, Controllers, reciben las peticiones HTTP y devuelve responses; la capa de aplicación, services, es donde se ejecuta como tal la simulación, solo conoce la lógica de la aplicación, lo como tal la lógica pura del dominio, eso se ocupa la capa Domain, DomainModels, dónde están las entidades (con las reglas) y enums; la capa de persistencia, que sería nuestra Base de datos JSON; la capa de Logging, que es donde se registra eventos y facilita la trazabilidad y la capa de DTOS que estructuran la comunicación entre cliente y servidor.
7. Comunicación entre componentes
La comunicación entre Unity_Dune y Servidor_dune se realiza mediante peticiones HTTP, siguiendo un esquema de API.

Unity envía solicitudes a los Controllers del servidor para crear partidas, ejecutar rondas, guardar el estado o consultar información. El servidor procesa estas solicitudes y devuelve respuestas utilizando los DTOs definidos en Responses.

El intercambio de datos se realiza en formato JSON, lo que facilita convertirlos para poder mandarlos y leerlos fácilmente entre el cliente y el servidor. Es decir, permite una serialización sencilla y asegura la compatibilidad entre ambos proyectos.
8. Justificación preliminar
La arquitectura propuesta permite separar responsabilidades de forma clara. Unity se centra en la interfaz y la interacción con el usuario, mientras que el servidor gestiona la lógica del dominio, la comunicación y la persistencia.

Aunque la solución se compone actualmente de dos proyectos, la estructura definida con Controllers, DomainModels, DTOs y Logging permite evolucionar hacia una arquitectura más distribuida, en la que distintos servicios pueden encargarse de tareas específicas como la simulación o la persistencia.

El uso de JSON simplifica la implementación en un principio y cumple con los requisitos de almacenamiento. Además, los DTOs permiten desacoplar la comunicación del modelo interno, facilitando, de esta forma, la evolución del sistema.
