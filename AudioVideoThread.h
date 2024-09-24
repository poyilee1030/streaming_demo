#include <QThread>
#include <QImage>
#include <QTime>

class AudioVideoThread : public QThread
{
    Q_OBJECT
public:
    AudioVideoThread(QString video_device_name, QString audio_device_name, int config_width, int config_height, int config_fps, int isPause);
    ~AudioVideoThread();

    void run() override;
    static void exit_av();
    static void pause_audio(int pause);
    static void reset_framecount();

signals:
    void updateDisplayImage(QImage img, double fps, bool updateFPS);
    void updateAudioBuffer(uint8_t *buffer, int bufferSize, int audio_frame_count, int storage_idx);
    void updateDisplayBuffer(unsigned char *buffer, int width, int height, double fps,
                             bool updateFPS, int video_frame_count, int storage_idx, int pts);

private:
    QString device_name;
    QString str_video_size;
    QString str_fps;
    //uint8_t *out_buffer_audio0;
    //uint8_t *out_buffer_audio1;
    unsigned char *out_buffer_video0;
    unsigned char *out_buffer_video1;
    unsigned char *out_buffer_video2;
};

