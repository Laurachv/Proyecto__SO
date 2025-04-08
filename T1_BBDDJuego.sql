DROP DATABASE IF EXISTS mistablas;
CREATE DATABASE mistablas;
USE mistablas;

-- Tabla de jugadores
CREATE TABLE jugadores (
    id_jugador INT AUTO_INCREMENT PRIMARY KEY,
    user VARCHAR(50),
    password VARCHAR(50),
    ganador TINYINT(1) DEFAULT 0,
    puntuacionjugador INT NOT NULL
);

-- Tabla de categorías
CREATE TABLE categorias (
    id_categoria INT AUTO_INCREMENT PRIMARY KEY,
    categoria VARCHAR(50)
);

-- Tabla de preguntas
CREATE TABLE preguntas (
    id_pregunta INT AUTO_INCREMENT PRIMARY KEY,
    pregunta VARCHAR(500),
    id_categoria INT,
    FOREIGN KEY (id_categoria) REFERENCES categorias(id_categoria)
        ON DELETE RESTRICT
        ON UPDATE CASCADE
);

-- Tabla de respuestas
CREATE TABLE respuestas (
    id_respuesta INT AUTO_INCREMENT PRIMARY KEY,
    respuestas VARCHAR(500),
    correctitud TINYINT(1) DEFAULT 0  -- 1 para correcta, 0 para incorrecta
);

-- Tabla de respuestas correctas
CREATE TABLE respuestascorrectas (
    id_correcta INT AUTO_INCREMENT PRIMARY KEY,
    id_respuesta INT NOT NULL,
    FOREIGN KEY (id_respuesta) REFERENCES respuestas(id_respuesta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE
);

-- Tabla de respuestas incorrectas
CREATE TABLE respuestasincorrectas (
    id_incorrecta INT AUTO_INCREMENT PRIMARY KEY,
    id_respuesta INT NOT NULL,
    FOREIGN KEY (id_respuesta) REFERENCES respuestas(id_respuesta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE
);

-- Tabla de puntos correctos
CREATE TABLE puntoscorrecta (
    id_correcta INT PRIMARY KEY,
    puntuacion_correcta INT DEFAULT 100,
    FOREIGN KEY (id_correcta) REFERENCES respuestascorrectas(id_correcta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE
);

-- Tabla de puntos incorrectos
CREATE TABLE puntosincorrecta (
    id_incorrecta INT PRIMARY KEY,
    puntuacion_incorrecta INT DEFAULT -50,
    FOREIGN KEY (id_incorrecta) REFERENCES respuestasincorrectas(id_incorrecta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE
);

-- Tabla de relación entre preguntas y respuestas
CREATE TABLE preguntarespuestas (
    id_pregunta INT,
    id_respuesta INT,
    id_correcta INT,  -- Relación con respuestascorrectas
    id_incorrecta INT, -- Relación con respuestasincorrectas
    FOREIGN KEY (id_pregunta) REFERENCES preguntas(id_pregunta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE,
    FOREIGN KEY (id_respuesta) REFERENCES respuestas(id_respuesta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE,
    FOREIGN KEY (id_correcta) REFERENCES respuestascorrectas(id_correcta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE,
    FOREIGN KEY (id_incorrecta) REFERENCES respuestasincorrectas(id_incorrecta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE
);

-- Tabla de relación entre categorías y preguntas
CREATE TABLE categoriapreguntas (
    id_categoria INT,
    id_pregunta INT,
    FOREIGN KEY (id_categoria) REFERENCES categorias(id_categoria)
        ON DELETE RESTRICT
        ON UPDATE CASCADE,
    FOREIGN KEY (id_pregunta) REFERENCES preguntas(id_pregunta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE
);

-- Tabla de puntuación total
CREATE TABLE puntuacion (
    id_jugador INT,
    puntuacion_total INT DEFAULT 0,
    FOREIGN KEY (id_jugador) REFERENCES jugadores(id_jugador)
        ON DELETE CASCADE
        ON UPDATE CASCADE
);

-- Tabla de puntuación respuesta (asociación de respuestas con puntos)
CREATE TABLE puntuacionrespuesta (
    id_pregunta INT,
    id_respuesta INT,
    puntos INT,
    FOREIGN KEY (id_pregunta) REFERENCES preguntas(id_pregunta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE,
    FOREIGN KEY (id_respuesta) REFERENCES respuestas(id_respuesta)
        ON DELETE RESTRICT
        ON UPDATE CASCADE
);

-- Tabla de partidas
CREATE TABLE partidas (
    id_partida INT AUTO_INCREMENT PRIMARY KEY,
    partida_terminada TINYINT(1) DEFAULT 0 -- Indicador de si la partida ha terminado
);

-- Tabla de relación entre jugadores y partidas (n jugadores pueden estar en una partida)
CREATE TABLE partida_jugadores (
    id_partida INT,
    id_jugador INT,
    FOREIGN KEY (id_partida) REFERENCES partidas(id_partida)
        ON DELETE CASCADE
        ON UPDATE CASCADE,
    FOREIGN KEY (id_jugador) REFERENCES jugadores(id_jugador)
        ON DELETE CASCADE
        ON UPDATE CASCADE,
    PRIMARY KEY (id_partida, id_jugador) -- Clave primaria compuesta
);

-- Procedimiento para seleccionar una categoría
DELIMITER //
CREATE PROCEDURE seleccionar_categoria(IN jugador_id INT)
BEGIN
    DECLARE categoria_id INT;

    -- El jugador elige una categoría. La categoría puede ser pasada como parámetro.
    -- Esto puede ser implementado por el cliente o el servidor, basado en la elección del jugador.
    -- La categoría se almacena en categoria_id.

    -- Llamar al procedimiento para obtener una pregunta aleatoria de esa categoría
    CALL obtener_pregunta_aleatoria(categoria_id, jugador_id);
END //
DELIMITER ;

-- Procedimiento para obtener una pregunta aleatoria
DELIMITER //
CREATE PROCEDURE obtener_pregunta_aleatoria(IN categoria_id INT, IN jugador_id INT)
BEGIN
    DECLARE pregunta_id INT;

    -- Seleccionar una pregunta aleatoria de la categoría
    SELECT id_pregunta INTO pregunta_id
    FROM preguntas
    WHERE id_categoria = categoria_id
    ORDER BY RAND()
    LIMIT 1;

    -- Llamar al procedimiento para mostrar las respuestas asociadas a la pregunta
    CALL mostrar_respuestas(pregunta_id, jugador_id);
END //
DELIMITER ;

-- Procedimiento para mostrar las respuestas de una pregunta
DELIMITER //
CREATE PROCEDURE mostrar_respuestas(IN pregunta_id INT, IN jugador_id INT)
BEGIN
    DECLARE respuesta_id INT;
    DECLARE correctitud TINYINT(1);

    -- Seleccionar las respuestas posibles para la pregunta
    DECLARE respuesta_cursor CURSOR FOR
    SELECT r.id_respuesta, r.correctitud
    FROM respuestas r
    JOIN preguntarespuestas pr ON r.id_respuesta = pr.id_respuesta
    WHERE pr.id_pregunta = pregunta_id;

    -- Abrir el cursor para recorrer las respuestas
    OPEN respuesta_cursor;

    -- Obtener respuestas y procesarlas
    FETCH respuesta_cursor INTO respuesta_id, correctitud;
    CLOSE respuesta_cursor;

    -- En un sistema real, el jugador elegiría una respuesta. Aquí se simula.
    -- Se debe recibir el valor de respuesta_id del jugador.

    -- Llamar al procedimiento para verificar la respuesta del jugador
    CALL verificar_respuesta(respuesta_id, jugador_id);
END //
DELIMITER ;

-- Procedimiento para verificar si la respuesta seleccionada es correcta o incorrecta
DELIMITER //
CREATE PROCEDURE verificar_respuesta(IN respuesta_id INT, IN jugador_id INT)
BEGIN
    DECLARE correctitud TINYINT(1);
    DECLARE puntos INT;

    -- Verificar si la respuesta es correcta o incorrecta
    DECLARE correcta_cursor CURSOR FOR
    SELECT r.correctitud
    FROM respuestas r
    WHERE r.id_respuesta = respuesta_id;

    OPEN correcta_cursor;
    FETCH correcta_cursor INTO correctitud;
    CLOSE correcta_cursor;

    -- Asignar puntos: 100 si es correcta, -50 si es incorrecta
    IF correctitud = 1 THEN
        SET puntos = 100;  -- Puntuación por respuesta correcta
    ELSE
        SET puntos = -50;  -- Puntuación por respuesta incorrecta
    END IF;

    -- Actualizar la puntuación del jugador
    UPDATE puntuacion
    SET puntuacion_total = GREATEST(puntuacion_total + puntos, 0)
    WHERE id_jugador = jugador_id;

    -- Lógica para determinar el ganador si algún jugador alcanza 1000 o 1050 puntos
    CALL actualizar_ganador(jugador_id);
END //
DELIMITER ;

-- Procedimiento para determinar el ganador
DELIMITER //
CREATE PROCEDURE actualizar_ganador(IN jugador_id INT, OUT resultado JSON)
BEGIN
    DECLARE max_puntos INT;
    DECLARE num_ganadores INT;
    DECLARE ganadores JSON;

    -- Obtener el jugador con la puntuación más alta
    SELECT MAX(puntuacion_total) INTO max_puntos FROM puntuacion;

    -- Contar cuántos jugadores tienen esa puntuación
    SELECT COUNT(*) INTO num_ganadores FROM puntuacion WHERE puntuacion_total = max_puntos;

    IF num_ganadores = 1 THEN
        -- Si hay solo un ganador, devolver el ID del jugador ganador
        SET ganadores = (SELECT JSON_OBJECT('id_jugador', id_jugador, 'puntuacion_total', puntuacion_total) 
                         FROM puntuacion WHERE puntuacion_total = max_puntos);
        SET resultado = JSON_OBJECT('estado', 0, 'ganadores', ganadores);
    ELSEIF num_ganadores <= 3 THEN
        -- Si hay más de un ganador pero hasta 3, devolver el código para empate
        SET resultado = JSON_OBJECT('estado', 2, 'mensaje', 'Empate, hasta 3 ganadores');
    ELSE
        -- Si hay más de 3 ganadores, devolver el código de empate
        SET resultado = JSON_OBJECT('estado', 1, 'mensaje', 'Demasiados ganadores, no se puede determinar');
    END IF;
END //
DELIMITER ;

