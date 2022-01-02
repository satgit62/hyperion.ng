#pragma once

// utils
#include <utils/Logger.h>
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/Components.h>
#include <utils/VideoMode.h>
#include <utils/settings.h>

#include <grabber/GrabberType.h>

class Grabber;
class QTimer;
class Hyperion;

///
/// Creates and destroys Grabber instances and moves the grabber to a thread. Pipes all signal/slots and methods to Grabber instance
///
class GrabberWrapper : public QObject
{
	Q_OBJECT
public:
	explicit GrabberWrapper(Hyperion* hyperion);
	~GrabberWrapper() override;
	///
	/// @brief Constructs a new Grabber, moves to thread and start
	/// @param config  The given configuration
	/// @param type    The settings type
	///
	void createGrabber(const QJsonObject& config, settings::type type);

	// ///
	// /// Check if grabber is active
	// ///
	// virtual bool isActive() const;

	// ///
	// /// @brief Get active grabber name
	// /// @param hyperionInd The instance index
	// /// @param type Filter for a given grabber type
	// /// @return Active grabbers
	// ///
	// virtual QStringList getActive(int inst, GrabberTypeFilter type = GrabberTypeFilter::ALL) const;

	static QStringList availableGrabbers(GrabberTypeFilter type = GrabberTypeFilter::ALL);

public slots:
	///
	/// @brief Handle settings update event from Hyperion
	/// @param type   settingsType from enum
	/// @param config configuration object
	///
	void settingsChanged(settings::type type, const QJsonDocument& config);

	///
	/// @brief Handle new component state request
	/// @param component  The comp from enum
	/// @param state      The new state
	///
	void componentStateChanged(hyperion::Components component, bool state);

signals:
	///
	/// @brief A new videoMode was requested
	///
	void newVideoMode(VideoMode mode);

	///
	/// @brief PIPE screen grabber images from GrabberWrapper to Hyperion class
	/// @param name   The name of the screeen grabber that is currently active
	/// @param image  The prepared image
	///
	void setScreenImage(const QString& name, const Image<ColorRgb>&  image);

	///
	/// @brief PIPE video grabber images from GrabberWrapper to Hyperion class
	/// @param name   The name of the video grabber that is currently active
	/// @param image  The prepared image
	///
	void setVideoImage(const QString& name, const Image<ColorRgb>& image);

	void stopScreenGrabber();
	void stopVideoGrabber();

private:
	void setScreenGrabberEnable(bool enable);
	void setVideoGrabberEnable(bool enable);

private slots:
	///
	/// @brief forward screen image to hyperion
	/// @param name  The grabber name
	/// @param image The image
	///
	void handleScreenImage(const QString& name, const Image<ColorRgb>& image);

	///
	/// @brief forward video image to hyperion
	/// @param name  The grabber name
	/// @param image The image
	///
	void handleVideoImage(const QString& name, const Image<ColorRgb> & image);

	///
	/// @brief Is called from _screenGrabberInactiveTimer to set source after specific time to inactive
	///
	void setScreenGrabberInactive();

	///
	/// @brief Is called from _videoGrabberInactiveTimer to set source after specific time to inactive
	///
	void setVideoGrabberInactive();

// 	///
// 	/// @brief Update Update capture rate
// 	/// @param type   interval between frames in milliseconds
// 	///
// 	void updateTimer(int interval);

protected:
	///
	/// @brief Stops the grabber thread
	/// @param type settingsType from enum
	///
	void stopGrabberThread(settings::type type);

private:
	// parent Hyperion
	Hyperion* _hyperion;

	// Pointer of current screen grabber with current state, name, priority and inactive timer
	Grabber*	_screenGrabber;
	bool		_screenGrabberEnabled;
	quint8		_screenGrabberPriority;
	QString		_screenGrabberName;
	QTimer*		_screenGrabberInactiveTimer;

	// Pointer of current video grabber with current state, name, priority and inactive timer
	Grabber*	_videoGrabber;
	bool		_videoGrabberEnabled;
	quint8		_videoGrabberPriority;
	QString		_videoGrabberName;
	QTimer*		_videoGrabberInactiveTimer;

	// /// The Logger instance
	// Logger * _log;
};
