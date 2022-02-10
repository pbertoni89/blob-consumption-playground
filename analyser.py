#!/user/bin/env python3

import pandas
import os
import re
from matplotlib import pyplot as plt


def main(fn: dir):
    re_cmd = re.compile(r'--elf (\w+) --inms (\d+) --outms (\d+) --njobs (\d+) --debug \d --nblobs (\d+)')
    re_alpha = re.compile(r'.*alpha.*-> (\d+).*-> (\d+).*\[s] (\d+)')
    re_omega = re.compile(r'.*omega.*\[ms] (\d+).*')
    dd = {'elf': [], 'inms': [], 'outms': [], 'njobs': [], 'nblobs': [], 'exp_rate_i': [], 'exp_rate_o': [], 'exp_time': [], 'act_time': []}
    df = pandas.DataFrame(dd)
    print(df)

    with open(fn, 'r') as fo:
        elf, inms, outms, njobs, nblobs, exp_rate_i, exp_rate_o, exp_time, act_time = (None for _ in range(9))
        for line in fo.readlines():
            ln = line.strip()
            m_cmd = re_cmd.match(ln)
            if m_cmd:
                elf, inms, outms, njobs, nblobs = m_cmd.groups()
                # print(f'C {m_cmd.groups()}')
            else:
                m_alpha = re_alpha.match(ln)
                if m_alpha:
                    exp_rate_i, exp_rate_o, exp_time = m_alpha.groups()
                    # print(f'A {m_alpha.groups()}')
                else:
                    m_omega = re_omega.match(ln)
                    if m_omega:
                        act_time, = m_omega.groups()
                        # print(f'O {m_omega.groups()}')
                        ls_row = [elf, inms, outms, njobs, nblobs, exp_rate_i, exp_rate_o, exp_time, act_time]
                        row = pandas.Series(ls_row, index=df.columns)
                        # print(f'appending {row}')
                        df = df.append(row, ignore_index=True)
    print(df.to_string())
    dtypes = {d: int for d in ['inms', 'outms', 'njobs', 'nblobs', 'exp_rate_i', 'exp_rate_o', 'exp_time', 'act_time']}
    dtypes['elf'] = 'category'
    df = df.astype(dtypes)
    df['act_time'] /= 1000  # they are milliseconds
    # df.plot(y='act_time')
    # plt.plot(df['act_time'])
    f, ax = plt.subplots(1)
    df_p = df.loc[df['elf'] == 'pool']
    df_a = df.loc[df['elf'] == 'async']
    ax.scatter(x=df_p['njobs'], y=df_p['exp_time'], c='black')
    ax.scatter(x=df_a['njobs'], y=df_a['exp_time'], c='red') #, s=df['act_time'])
    plt.xlabel('njobs')
    plt.ylabel('exp_time')
    f, ax = plt.subplots(1)
    ax.scatter(x=df['act_time'], y=df['exp_time'])
    plt.show()


if __name__ == '__main__':
    main(f'{os.path.join(os.path.dirname(__file__), "results-and-stderr.txt")}')
