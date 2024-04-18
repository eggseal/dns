# Proyecto DNS


## Introduccion

Este proyecto consiste en un cliente y servidor DNS capaces de comunicarse entre si o con otros programas de DNS. 

Estos programas estas hechos en C y usan la API de sockets de Berkeley

El programa de cliente es capaz de comunicarse con cualquier servidor DNS, logear la direccion de respuesta y mostrarla ordenada el la pantalla, este cuenta con un sistema de cache que almacena las ultimas 10 peticiones realizadas para poder ahorrar la comunicacion con el servidor

El programa de servidor es capaz de recibir y procesar peticiones DNS de un cliente y generar una respuesta de acuerdo a la informacion almacenada en su archivo de zonas DNS, este programa una vez ejecutado puede ser usado como el DNS de un dispositivo para navegar por internet a los dominios registrados. Incluso si el dominio y la direccion no son correctos, siempre y cuando ninguno de los dos cuente con certificacion HTTPS.

El servidor puede ser ejecutado localmente y otros dispositivos en la misma red pueden conectarse a el usando su IP privada, o puede ser desplegado en la nube para que cualquier dispositivo se conecte a el.

## Desarrollo

El desarrollo del proyecto fue hecho en Linux usando la API de sockets de Berkeley `<sys/socket.h>`.

Se empezo desarrollando en cliente para poder entender mejor la estructura de las peticiones y respuestas de un DNS completo como el servidor `8.8.8.8` de Google, esto brindo un mejor entendimiento de los componentes de la peticion, los tipos y clases de peticion y del significado de cada set de bytes y como afectan a la peticion.

Luego de verificar la funcionalidad del cliente comparandola con la salida del comando de Linux `dig`, ya teniamos un mayor conocimiento de la estructura de una peticion y respuesta del DNS.

El desarrollo del servidor fue mas rapido gracias al conocimiento adquirido por el desarrollo del cliente, ya sabiamos el orden y significado de cada byte, solo era asunto de saber manejar el buffer

## Aspectos logrados

- **Servidor:**

  Para el servidor logramos hacer la conexion al socket con el protocolo UDP y lo vinculamos al puerto indicado por el usuario, donde esta constantemente en espera de una peticion

  La correcta lectura y estructura de un archivo de zona DNS donde se almacenan los dominion que gestiona el servidor

  La implementacion del gestionador de peticiones que recibe una peticion DNS del cliente y la responde correctamente usando la informacion del archivo de zona

- **Cliente:**

  Para el cliente logramos realizar el envio de datos al socket coon el protocolo UDP

  La construccion de la peticion al DNS, haciendo uso de estructuras propias que facilitan la manipulacion de los bits.

  La creacion del historial de peticiones realizadas almacenado en una ruta especificada por el comando de ejecucion

  La visualizacion organizada por consola de la respuesta del servidor DNS

  La creacion del cache de peticiones y el comando `FLUSH` para gestionarlas

## Aspectos no logrados

- **Servidor:**

  La concurrencia en la comunicacion de los sockets para la respuesta de multiples peticiones en simultaneo

  La creacion del historial de peticiones recibidas, en parte debido a la falta de especificacion de los elementos a logear en el enunciado

## Conclusiones

## Referencias

**Para el funcionamiento del sockets con UDP**

[UDP Server-Client Implementation in C++](https://www.geeksforgeeks.org/udp-server-client-implementation-c/) - geeksforgeeks.org

**Para la estructura e implementacion de una peticion y respuesta DNS**

[DOMAIN NAMES - IMPLEMENTATION AND SPECIFICATION](https://www.ietf.org/rfc/rfc1035.txt) - ietf.org

**Para referencia de una implementacion funcional del cliente**

[DNS Query Code in C with linux sockets](https://gist.github.com/fffaraz/9d9170b57791c28ccda9255b48315168) - gist.github.com
