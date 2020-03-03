from udls import SimpleDataset
import librosa as li
from . import config
import numpy as np
import torch


def preprocess(name):
    try:
        x = li.load(name, config.SAMPRATE)[0]
    except:
        return None

    border = len(x) % config.N_SIGNAL

    if len(x) < config.N_SIGNAL:
        return None

    elif border:
        x = x[:-border]

    x = x.reshape(-1, config.N_SIGNAL)

    if config.EXTRACT_LOUDNESS and config.TYPE == "vanilla":
        dim_reduction = config.HOP_LENGTH * np.prod(config.RATIOS)
        x_win = x.reshape(x.shape[0], -1, dim_reduction)
        win = np.hanning(dim_reduction)
        win /= np.mean(win)
        win = win.reshape(1, 1, -1)
        x_win = x_win * win
        eps = 1e-4
        log_rms = .5 * np.log(np.clip(np.mean(x_win**2, -1), eps, 1))
        log_rms = (np.log(eps) - 2 * log_rms) / np.log(eps)

        x = zip(x, log_rms)

    return x


class Loader(torch.utils.data.Dataset):
    def __init__(self, cat, config=config):
        super().__init__()
        self.dataset = SimpleDataset(config.LMDB_LOC,
                                     config.WAV_LOC.split(","),
                                     preprocess,
                                     map_size=1e11)
        self.cat = cat

    def __len__(self):
        return len(self.dataset)

    def __getitem__(self, idx):
        return torch.cat([
            torch.from_numpy(self.dataset[(idx + i) % self.__len__()]).float()
            for i in range(self.cat)
        ], -1)
