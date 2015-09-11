/**
 * FILE:		pcmaudio.h
 *
 * DESCRIPTION:
 *
 * This is the class for receiving and playing pcm audio
 * -----------------------------------------------------------------------
 *    Copyright (C) 2010-2015 OpenNetcam Project.
 *
 *   This  software is released under the following license:
 *        - GNU General Public License (GPL) version 3 for use with the
 *          Qt Open Source Edition (http://www.qt.io)
 *
 *    Permission to use, copy, modify, and distribute this software and its
 *    documentation for any purpose and without fee is hereby granted
 *    in accordance with the provisions of the GPLv3 which is available at:
 *    http://www.gnu.org/licenses/gpl.html
 *
 *    This software is provided "as is" without express or implied warranty.
 *
 * -----------------------------------------------------------------------
 */

//#define QTMULTIMEDIA 1

#ifndef PCMAUDIO_H
#define PCMAUDIO_H
#include <QByteArray>
#include <QBuffer>
#ifdef QTMULTIMEDIA
#include <QAudioOutput>
#else
#ifdef QTPHONON
#include <Phonon/AudioOutput>
#endif
#endif

#include "avformat.h"
#include "aviformat.h"

// pcm conversion
#define SIGN_BIT (0x80) /* Sign bit for a A-law byte. */
#define QUANT_MASK (0xf) /* Quantization field mask. */
// #define NSEGS (8) /* Number of A-law segments. */
#define SEG_SHIFT (4) /* Left shift for segment number. */
#define SEG_MASK (0x70) /* Segment field mask. */
#define BIAS (0x84) /* Bias for linear code. */

class pcmAudio : QObject
{
      Q_OBJECT
public:
    pcmAudio(QObject *parent);
    ~pcmAudio();

    void setFormat();
    void setData( QByteArray & data, AvFormat *av=NULL );
    bool ulaw2linear(const char *data, int len, AvFormat *av);
protected slots:
#ifdef QTMULTIMEDIA
    void finishedPlaying(QAudio::State state);
#endif
private:
    QIODevice *buffer;
#ifdef QTMULTIMEDIA
    QAudioFormat format;
    QAudioOutput *audio;
#else
    void * audio;
#endif
#ifdef QTPHONON
    Phonon::AudioOutput *audioOutput;
#endif
};

#endif // PCMAUDIO_H
