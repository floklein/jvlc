#ifndef VLC_HOTKEY_CONVERTER_HPP
#define VLC_HOTKEY_CONVERTER_HPP

#include <QObject>
#include <QPoint>
#include <QtQml/qqmlregistration.h>

/* VLC Key/Wheel hotkeys interactions */

Q_MOC_INCLUDE("QKeyEvent")
Q_MOC_INCLUDE("QWheelEvent")

class QKeyEvent;
class QWheelEvent;
class QInputEvent;

int qtKeyModifiersToVLC( const QInputEvent& e );
int qtEventToVLCKey( QKeyEvent *e );
int qtWheelEventToVLCKey( const QWheelEvent& e );
QString VLCKeyToString( unsigned val, bool );

struct vlc_object_t;

/**
 * @brief the first key currently bound to a hotkey action, for display
 * @param obj object the key bindings are inherited from
 * @param action action name without the "key-" prefix, e.g. "play-pause"
 * @return the translated key ("Space", "Shift+R"), or an empty string if
 * the action has no key
 */
QString vlcActionKeyText( struct vlc_object_t *obj, const char *action );

/**
 * @brief the key sequence of an action of the interface itself (opening a
 * dialog, showing a view...), as opposed to the core hotkeys
 * @param action action name, e.g. "open-folder"
 * @return a portable key sequence ("Ctrl+Shift+O"), or an empty string if
 * @a action is not an interface action
 */
QString appActionSequence( const QString &action );

/**
 * @brief the key shown next to an action in menus and tooltips
 * @param obj object the core key bindings are inherited from
 * @param action a core hotkey action ("play-pause") or an interface action
 * ("open-folder")
 * @return the key ("Space", "Ctrl+Shift+O"), or an empty string if the
 * action has no key
 */
QString actionKeyText( struct vlc_object_t *obj, const QString &action );

/**
 * @brief The WheelToVLCConverter class aggregates wheel events and
 * emit a signal once it gathers a full scroll step, as VLC doesn't handle
 * fractionnal scroll events
 */
class WheelToVLCConverter : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    using QObject::QObject;

public:
    Q_INVOKABLE Qt::Orientations getWheelOrientation(int x, int y);

signals:
    /**
     * @param vlcKey the VLC hotkey representation
     */
    void vlcWheelKey(int vlcKey);
    /**
     * @param steps: positive value indicated UP wheel events, negative DOWN wheel event
     * @param modifiers are keyboard pressed modifiers
     */
    void wheelUpDown(int steps, Qt::KeyboardModifiers modifiers);
    /**
     * @param steps: positive value indicated UP wheel events, negative DOWN wheel event
     * @param modifiers are keyboard pressed modifiers
     */
    void wheelLeftRight(int steps, Qt::KeyboardModifiers modifiers);

public slots:
    /**
     * @brief qmlWheelEvent handles wheel events as emitted by QWidget
     * @param e the wheel event
     */
    void wheelEvent(const QWheelEvent* e);

    /**
     * @brief qmlWheelEvent handles wheel events as emitted by QML
     * @param wheelEvent qml wheel event
     */
    void qmlWheelEvent(const QObject* wheelEvent);

    void customWheelEvent(const QPoint& pixelDelta,
                          const QPoint& angleDelta,
                          const Qt::MouseButtons buttons = {},
                          const Qt::KeyboardModifiers modifiers = {},
                          const bool inverted = false);

private:
    QPoint m_scrollAmount = {};
    Qt::KeyboardModifiers m_modifiers = {};
    Qt::MouseButtons m_buttons = {};
};

#endif // VLC_HOTKEY_CONVERTER_HPP
